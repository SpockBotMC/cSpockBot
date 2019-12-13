#include <stdint.h>
#include <uv.h>
#include "1_14_4_proto.h"
#include "sds.h"
#include "sb_event.h"
#include "logc/log.h"

#define ERR_RET(ret, x) do { if((ret = x) < 0) return ret;} while(0)

#define CHK_ALLOC(x) do {                                                     \
  if(!(x)) {                                                                  \
    log_error("Failed to allocate");                                          \
    exit(-1);                                                                 \
  }                                                                           \
} while(0)

typedef struct {
  sds addr;
  int port;
} sbnet_settings;

typedef struct {
  sbnet_settings settings;
  uint64_t *handles[protocol_state_max][protocol_direction_max];
  uv_loop_t loop;
  uv_tcp_t tcp;
} sbnet_netcore;

void sbnet_start_cb(vgc_fiber fiber, void *cb_data, void *event_data,
                    uint64_t handle) {
  sbnet_netcore *net = cb_data;

}

static void sbnet_handles_alloc(
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

static void sbnet_handles_reg(
  uint64_t *handles[protocol_state_max][protocol_direction_max],
  sbev_eventcore *ev) {
  for(int state = 0; state < protocol_state_max; state++)
    for(int direction = 0; direction < protocol_direction_max; direction++)
      for(int id = 0; id < protocol_max_ids[state][direction]; id++)
        handles[state][direction][id] = sbev_reg_event(
          ev, protocol_strings[state][direction][id]
        );
}

int sbnet_net_init(sbnet_netcore *net, sbev_eventcore *ev,
                   sbnet_settings settings) {
  net->settings = settings;
  int ret;
  ERR_RET(ret, uv_loop_init(&net->loop));
  ERR_RET(ret, uv_tcp_init(&net->loop, &net->tcp));
  sbnet_handles_alloc(net->handles);
  sbnet_handles_reg(net->handles, ev);

  sbev_reg_event_cb(ev, "start", sbnet_start_cb, net);
  return 0;
}

void sbnet_net_free(sbnet_netcore *net) {
  for(int state = 0; state < protocol_state_max; state++)
    for(int direction = 0; direction < protocol_direction_max; direction++)
      free(net->handles[state][direction]);
}
