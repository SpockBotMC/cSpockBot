#include <stdint.h>
#include <uv.h>
#include "1_14_4_proto.h"
#include "sds.h"
#include "sb_event.h"
#include "sb_net.h"
#include "logc/log.h"
#include "assert.h"

#define ERR_RET(ret, x) do { if((ret = x) < 0) return ret;} while(0)

#define CHK_ALLOC(x) do {                                                     \
  if(!(x)) {                                                                  \
    log_error("Failed to allocate");                                          \
    exit(-1);                                                                 \
  }                                                                           \
} while(0)

#define NO_SIZE -1

enum compression_state {
  uncompressed,
  compressed
};

int sbnet_recv_bbuf(char *dest, sbnet_bbuf *src, size_t len) {
  if(src->in_use < len)
    return -1;
  memcpy(dest, src->cur, len);
  src->cur += len;
  src->in_use -= len;
  return 0;
}

int sbnet_adv_bbuf(sbnet_bbuf *buf, size_t dist) {
  if(buf->in_use < dist)
    return -1;
  buf->cur += dist;
  buf->in_use -= dist;
  return 0;
}

void sbnet_bbuf_make_room(sbnet_bbuf *buf, size_t len) {
  if (len <= buf->rem)
    return;
  buf->cur = memmove(buf->base, buf->cur, buf->in_use);
  buf->rem = buf->len - buf->in_use;
  if (len <= buf->rem)
    return;
  buf->len = buf->in_use + len;
  buf->rem = len;
  CHK_ALLOC(buf->base = realloc(buf->base, buf->len));
  buf->cur = buf->base;
}

uv_buf_t sbnet_buf_init(size_t size) {
  char *tmp = malloc(size);
  CHK_ALLOC(tmp);
  return uv_buf_init(tmp, size);
}

static void sbnet_ontick(vgc_fiber *fiber, void *cb_data, void *event_data,
                         uint64_t handle) {
  sbnet_netcore *net = cb_data;
  net->fiber_ptr = fiber;
  uv_run(&net->loop, UV_RUN_ONCE);
}

static void sbnet_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
  sbnet_netcore *net = handle->data;
  sbnet_bbuf_make_room(&net->read_buf, size);
  *buf = uv_buf_init(net->read_buf.cur + net->read_buf.in_use, size);
}

static void sbnet_read_cb(uv_stream_t *s, ssize_t nread, const uv_buf_t *buf) {
  sbnet_netcore *net = s->data;
  if(nread < 0) {
    log_error("Error on read");
    exit(-1);
  } else if (nread == UV_EOF) {
    log_debug("Socket EOF");
    sbev_kill(net->ev);
    return;
  }

  net->read_buf.in_use += nread;
  net->read_buf.rem -= nread;
  int ret;
  for(;;) {
    if(net->next_packet_size == NO_SIZE) {
      ret = walk_varint(net->read_buf.cur, net->read_buf.in_use);
      if(ret == varnum_overrun) {
        break;
      } else if(ret == varnum_invalid) {
        log_error("Invalid packet size varint");
        exit(-1);
      }
      int32_t packet_size;
      dec_varint(&packet_size, net->read_buf.cur);
      sbnet_adv_bbuf(&net->read_buf, ret);
      net->next_packet_size = packet_size;
    }

    if(net->next_packet_size > net->read_buf.in_use)
      break;

    // ToDo: Fire this off in a seperate fiber, so we can decode a new packet
    // while still resolving the old one
    char *data;
    size_t data_len;
    int32_t id;
    if(net->compression == uncompressed) {
      ret = walk_varint(net->read_buf.cur, net->read_buf.in_use);
      if(ret < 0) {
        log_error("Error decoding packet id");
        exit(-1);
      }
      dec_varint(&id, net->read_buf.cur);
      sbnet_adv_bbuf(&net->read_buf, ret);
      data_len = net->next_packet_size - ret;
      CHK_ALLOC(data = malloc(data_len));
      sbnet_recv_bbuf(data, &net->read_buf, data_len);
    } else {
      log_debug("Compression unimplemented");
      exit(-1);
    }
    void *pak = generic_toclient_decode(net->proto_state, id, data, data_len);
    if(!pak) {
      log_error("Error decoding packet: %s",
                 protocol_strings[net->proto_state][toclient_id][id]);
      exit(-1);
    }

    log_debug("Decoded: %s",
              protocol_strings[net->proto_state][toclient_id][id]);
    vgc_counter count;
    sbev_emit_event(net->ev, net->handles[net->proto_state][toclient_id][id],
                    pak, *net->fiber_ptr, &count);
    *net->fiber_ptr = vgc_wait_for_counter(*net->fiber_ptr, &count);
  }
}

static void sbnet_write_cb(uv_write_t *write, int status) {
  if(status) {
    log_error("Error on write: %s", uv_strerror(status));
    exit(-1);
  }
  free(write->data);
  free(write);
}

size_t sbnet_size_uncom_hdr(uint32_t src_size, uint32_t src_id) {
  size_t pid_size = size_varint(src_id);
  uint32_t total_size = src_size + pid_size;
  return size_varint(total_size) + pid_size;
}

char * sbnet_enc_uncom_hdr(char *dest, uint32_t src_size, uint32_t src_id) {
  size_t total_size = src_size + size_varint(src_id);
  dest = enc_varint(dest, total_size);
  return enc_varint(dest, src_id);
}

static void sbnet_connect_cb(uv_connect_t *connect, int status) {
  if(status < 0) {
    log_error("Error on connect: %s", uv_strerror(status));
    exit(-1);
  }
  sbnet_netcore *net = connect->data;
  connect->handle->data = net;
  handshaking_toserver_set_protocol set_proto = {
    .protocol_version = 498,
    .server_host = sdsnew(net->settings.addr),
    .server_port = 25565,
    .next_state = login_id
  };
  login_toserver_login_start login_start = {
    .username = sdsnew(net->settings.username)
  };

  size_t s_set_proto = size_handshaking_toserver_set_protocol(set_proto);
  size_t s_set_proto_hdr = sbnet_size_uncom_hdr(s_set_proto, handshaking_toserver_set_protocol_id);

  size_t s_login_start = size_login_toserver_login_start(login_start);
  size_t s_login_start_hdr = sbnet_size_uncom_hdr(s_login_start, login_toserver_login_start_id);

  size_t tot_size = s_set_proto_hdr + s_set_proto + s_login_start_hdr + s_login_start;
  uv_buf_t buf = sbnet_buf_init(tot_size);

  char *tmp = sbnet_enc_uncom_hdr(buf.base, s_set_proto, handshaking_toserver_set_protocol_id);
  tmp = enc_handshaking_toserver_set_protocol(tmp, set_proto);
  tmp = sbnet_enc_uncom_hdr(tmp, s_login_start, login_toserver_login_start_id);
  enc_login_toserver_login_start(tmp, login_start);

  uv_write_t *write = malloc(sizeof(*write));
  CHK_ALLOC(write);
  write->data = buf.base;
  net->proto_state = login_id;
  uv_write(write, connect->handle, &buf, 1, sbnet_write_cb);
  uv_read_start(connect->handle, sbnet_alloc_cb, sbnet_read_cb);

  free_handshaking_toserver_set_protocol(set_proto);
  free_login_toserver_login_start(login_start);
}

void sbnet_start_cb(vgc_fiber *fiber, void *cb_data, void *event_data,
                    uint64_t handle) {
  sbnet_netcore *net = cb_data;
  struct sockaddr_in dest;
  uv_ip4_addr(net->settings.addr, net->settings.port, &dest);
  uv_tcp_connect(&net->connect, &net->tcp, (struct sockaddr *) &dest,
                 sbnet_connect_cb);
}

void sbnet_compress_cb(vgc_fiber *fiber, void *cb_data, void *event_data,
                       uint64_t handle) {
  sbnet_netcore *net = cb_data;
  net->compression = compressed;
}

static void sbnet_alloc_handles(
  uint64_t *handles[protocol_state_max][protocol_direction_max]) {
  for(int state = 0; state < protocol_state_max; state++)
    for(int direction = 0; direction < protocol_direction_max; direction++)
        if(protocol_max_ids[state][direction])
          CHK_ALLOC(handles[state][direction] = malloc(
            protocol_max_ids[state][direction] * sizeof(***handles)
          ));
        else
          handles[state][direction] = NULL;
}

static void sbnet_reg_handles(
  uint64_t *handles[protocol_state_max][protocol_direction_max],
  sbev_eventcore *ev) {
  for(int state = 0; state < protocol_state_max; state++)
    for(int direction = 0; direction < protocol_direction_max; direction++)
      for(int id = 0; id < protocol_max_ids[state][direction]; id++)
        handles[state][direction][id] = sbev_reg_event(
          ev, protocol_strings[state][direction][id]
        );
}

int sbnet_init_net(sbnet_netcore *net, sbev_eventcore *ev,
                   sbnet_settings settings) {
  net->ev = ev;
  net->settings = settings;
  net->compression = uncompressed;
  net->proto_state = handshaking_id;
  net->next_packet_size = NO_SIZE;
  net->read_buf.len = 4096;
  CHK_ALLOC(net->read_buf.base = malloc(net->read_buf.len));
  net->read_buf.cur = net->read_buf.base;
  net->read_buf.in_use = 0;
  net->read_buf.rem = net->read_buf.len;
  int ret;
  ERR_RET(ret, uv_loop_init(&net->loop));
  ERR_RET(ret, uv_tcp_init(&net->loop, &net->tcp));
  net->connect.data = net;
  sbnet_alloc_handles(net->handles);
  sbnet_reg_handles(net->handles, ev);
  sbev_reg_event_cb(ev, "start", sbnet_start_cb, net);
  sbev_reg_event_cb(ev, "tick", sbnet_ontick, net);
  sbev_reg_event_cb(ev, "login_toclient_compress", sbnet_compress_cb, net);
  return 0;
}

void sbnet_free_net(sbnet_netcore *net) {
  for(int state = 0; state < protocol_state_max; state++)
    for(int direction = 0; direction < protocol_direction_max; direction++)
      free(net->handles[state][direction]);
}
