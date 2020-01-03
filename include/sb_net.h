#ifndef SB_NET_H
#define SB_NET_H

#include <stdint.h>
#include <uv.h>

#include "fibers/vgc.h"
#include "base.h"
#include "sb_event.h"
#include MC_PROTO_INCLUDE
#include "sds.h"


/*
| base |      | cur  |      | last |      |      |
| 0x00 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 |
<------------------  len = 7 -------------------->
               <---- in_use = 3 --->
                                    <- rem = 2 -->
*/

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
  CSB_PLUGIN_BASE
  sbev_eventcore *ev;
  // Keeping a copy of the fiber around is a %BAD% idea generally, and you
  // shouldn't do it. Net needs to do this because it has to interface with
  // libuv's asynchronous event loop and doesn't have anywhere to stash the
  // fiber across callbacks.
  vgc_fiber *fiber_ptr;
  sbnet_settings settings;
  uint64_t *handles[protocol_state_max][protocol_direction_max];
  int compression;
  int32_t threshold;
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
