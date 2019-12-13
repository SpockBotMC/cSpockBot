#ifndef SB_NET_H
#define SB_NET_H

#include <stdint.h>
#include <uv.h>

#include "fibers/vgc.h"
#include "sb_event.h"
#include "1_14_4_proto.h"
#include "sds.h"

typedef struct {
  char *base;
  char *cur;
  size_t len;
  size_t in_use;
  size_t rem;
} sbnet_bbuf;

typedef struct {
  char *addr;
  char *username;
  int port;
} sbnet_settings;

typedef struct {
  sbev_eventcore *ev;
  vgc_fiber fiber;
  sbnet_settings settings;
  uint64_t *handles[protocol_state_max][protocol_direction_max];
  int compression;
  int proto_state;
  sbnet_bbuf read_buf;
  ssize_t next_packet_size;
  uv_loop_t loop;
  uv_tcp_t tcp;
  uv_connect_t connect;
} sbnet_netcore;

int sbnet_init_net(sbnet_netcore *net, sbev_eventcore *ev,
                   sbnet_settings settings);

#endif
