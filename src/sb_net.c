#include <stdint.h>
#include <uv.h>
#include "sds.h"
#include "sb_event.h"

#define ERR_RET(ret, x) do { if((ret = x) < 0) return ret;} while(0)

typedef struct {
  sds addr;
  int port;
} sbnet_settings;

typedef struct {
  sbnet_settings settings;
  uv_loop_t loop;
  uv_tcp_t tcp;
} sbnet_netcore;

void sbnet_start_cb(vgc_fiber fiber, void *cb_data, void *event_data,
                    uint64_t handle) {
  // Do something
}

int sbnet_net_init(sbnet_netcore *net, sbev_eventcore *ev,
                   sbnet_settings settings) {
  net->settings = settings;
  int ret;
  ERR_RET(ret, uv_loop_init(&net->loop));
  ERR_RET(ret, uv_tcp_init(&net->loop, &net->tcp));
  sbev_reg_event_cb(ev, "start", sbnet_start_cb, net);
  return 0;
}
