#ifndef SB_EVENT_H
#define SB_EVENT_H

#include <stdint.h>
#include <uv.h>
#include "sds.h"
#include "uthash.h"
#include "fibers/vgc.h"

typedef void (*sbev_event_cb)(vgc_fiber fiber, void *cb_data, void *ev_data,
                              uint64_t handle);

typedef struct {
  sbev_event_cb cb;
  void *cb_data;
} sbev_cb_and_data;

typedef struct {
  uv_rwlock_t lock;
  sds nomen;
  uint64_t handle;
  sbev_cb_and_data *cbs;
  uint64_t max;
  uint64_t cur;
  UT_hash_handle hh;

  // Support for removing callbacks
  size_t free_max;
  size_t free_cur;
  uint64_t *free_handles;
} sbev_event_entry;

typedef struct {
  vgc_fiber fiber;
  uv_rwlock_t lock;
  // Event hashmap and lookup array
  sbev_event_entry *entries_map;
  sbev_event_entry *entries;
  // Array max and current size
  uint64_t max;
  uint64_t cur;
  // Event handles
  uint64_t start;
  uint64_t tick;
  uint64_t kill;
  // Misc
  int kill_flag;
} sbev_eventcore;

void sbev_init_event(sbev_eventcore *ev, vgc_fiber fiber);
uint64_t sbev_reg_event(sbev_eventcore *ev, char const *nomen);
uint64_t sbev_reg_cb(sbev_eventcore *ev, uint64_t ev_handle, sbev_event_cb cb,
                     void *cb_data);
void sbev_reg_event_cb(sbev_eventcore *ev, char const *nomen, sbev_event_cb cb,
                       void *cb_data);

void sbev_run_event_once(sbev_eventcore *ev);
void sbev_run_event_continous(sbev_eventcore *ev);
void sbev_start_event(sbev_eventcore *ev, int continuous);
void sbev_kill(sbev_eventcore *ev);
#endif
