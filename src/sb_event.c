#include <stdint.h>
#include "sds.h"
#include "uthash.h"
#include "logc/log.h"

#include "fibers/vgc.h"
#include "sb_event.h"

#define CHK_ALLOC(x) do {                                                     \
  if(!(x)) {                                                                  \
    log_error("Failed to allocate");                                          \
    exit(-1);                                                                 \
  }                                                                           \
} while(0)

#define ERR_CHK(x, str) do { if(x < 0) { log_error(str); exit(-1);}} while(0)

void sbev_init_event(sbev_eventcore *ev) {
  ERR_CHK(uv_rwlock_init(&ev->lock), "Failed to init rwlock");
  ev->entries_map = NULL;
  ev->max = 20; // Chosen by fair dice roll
  ev->cur = 0;
  CHK_ALLOC(ev->entries = malloc(sizeof(*ev->entries) * ev->max));
  ev->kill_flag = 0;
  ev->start = sbev_reg_event(ev, "start");
  ev->tick = sbev_reg_event(ev, "tick");
  ev->kill = sbev_reg_event(ev, "kill");
}

void sbev_init_event_entry(sbev_event_entry *ent, char const *nomen, uint64_t handle) {
  ERR_CHK(uv_rwlock_init(&ent->lock), "Failed to init rwlock");
  CHK_ALLOC(ent->nomen = sdsnew(nomen));
  ent->handle = handle;
  ent->max = 20; // These dice are rigged
  ent->cur = 0;
  ent->free_max = 20;
  ent->free_cur = 0;
  CHK_ALLOC(ent->cbs = malloc(sizeof(*ent->cbs) * ent->max));
  CHK_ALLOC(
    ent->free_handles = malloc(sizeof(*ent->free_handles) * ent->free_max)
  );
}

uint64_t sbev_reg_event(sbev_eventcore *ev, char const *nomen) {
  sbev_event_entry *find;
  uv_rwlock_rdlock(&ev->lock);
  HASH_FIND_STR(ev->entries_map, nomen, find);
  uv_rwlock_rdunlock(&ev->lock);
  if(find)
    return find->handle;

  uv_rwlock_wrlock(&ev->lock);
  if(ev->cur == ev->max) {
    ev->max *= 2;
    HASH_CLEAR(hh, ev->entries_map);
    CHK_ALLOC(ev->entries
              = realloc(ev->entries, sizeof(*ev->entries) * ev->max));
    for(uint64_t i = 0; i < ev->cur; i++)
        HASH_ADD_KEYPTR(hh, ev->entries_map, ev->entries[i].nomen,
                        sdslen(ev->entries[i].nomen), &ev->entries[i]);
  }
  uint64_t handle = ev->cur++;
  sbev_event_entry *ent = &ev->entries[handle];
  sbev_init_event_entry(ent, nomen, handle);
  HASH_ADD_KEYPTR(hh, ev->entries_map, ent->nomen, sdslen(ent->nomen), ent);
  uv_rwlock_wrunlock(&ev->lock);
  return handle;
}

uint64_t sbev_reg_cb(sbev_eventcore *ev, uint64_t ev_handle, sbev_event_cb cb,
                     void *cb_data) {
  uv_rwlock_rdlock(&ev->lock);
  sbev_event_entry *ent = &ev->entries[ev_handle];
  uv_rwlock_wrlock(&ent->lock);
  uint64_t ent_handle;
  if(ent->free_cur) {
    ent->free_cur--;
    ent_handle = ent->free_handles[ent->free_cur];
  } else {
    if(ent->cur == ent->max) {
      ent->max *= 2;
      sbev_cb_and_data *p = realloc(ent->cbs, sizeof(*ent->cbs) * ent->max);
      CHK_ALLOC(p);
      ent->cbs = p;
    }
    ent_handle = ent->cur;
    ent->cur++;
  }
  ent->cbs[ent_handle].cb = cb;
  ent->cbs[ent_handle].cb_data = cb_data;
  uv_rwlock_wrunlock(&ent->lock);
  uv_rwlock_rdunlock(&ev->lock);
  return ent_handle;
}

void sbev_unreg_cb(sbev_eventcore *ev, uint64_t ev_handle,
                   uint64_t ent_handle) {
  uv_rwlock_rdlock(&ev->lock);
  sbev_event_entry *ent = &ev->entries[ev_handle];
  uv_rwlock_wrlock(&ent->lock);
  if(ent->free_cur == ent->free_max) {
    ent->free_max *= 2;
    uint64_t *p = realloc(ent->free_handles,
                          sizeof(*ent->free_handles) * ent->free_max);
    CHK_ALLOC(p);
    ent->free_handles = p;
  }
  ent->cbs[ent_handle].cb = NULL;
  ent->free_handles[ent->free_cur] = ent_handle;
  ent->free_cur++;
  uv_rwlock_wrunlock(&ent->lock);
  uv_rwlock_rdunlock(&ev->lock);
}

void sbev_reg_event_cb(sbev_eventcore *ev, char const *nomen, sbev_event_cb cb,
                       void *cb_data) {
  // ToDo: Should we have unsafe versions of these so we don't grab the locks
  // multiple times?
  uint64_t handle = sbev_reg_event(ev, nomen);
  sbev_reg_cb(ev, handle, cb, cb_data);
}

// Should sbev even be wrapping fibers in a trampoline? Might be better to just
// let people deal with fibers directly
typedef struct {
  sbev_cb_and_data cbs;
  void *ev_data;
  uint64_t handle;
} sbev_tramp_arg;

void sbev_trampoline(vgc_fiber fiber) {
  sbev_tramp_arg * tmp = fiber.data;
  tmp->cbs.cb(&fiber, tmp->cbs.cb_data, tmp->ev_data, tmp->handle);
  free(tmp);
  vgc_fiber_finish(fiber);
}

// We're always in an event, so locking to emit would lead to a deadlock
// nightmare. We need to operate on a local copy. The python version has the
// exact same problem. This has performance implications, we will need to
// revist this at some point.

// We don't really need this fiber, we just need a pointer to the scheduler
// which we could store in the eventcore. Reconsider this
void sbev_emit_event(sbev_eventcore *ev, uint64_t handle, void *ev_data,
                     vgc_fiber fiber, vgc_counter *count) {
  uv_rwlock_rdlock(&ev->lock);
  sbev_event_entry *ent = &ev->entries[handle];
  uv_rwlock_rdlock(&ent->lock);
  uint64_t cur = ent->cur;
  sbev_cb_and_data *cbs;

  // Early exit if there are no callbacks registered for this event
  if(!cur) {
    uv_rwlock_rdunlock(&ev->entries[handle].lock);
    uv_rwlock_rdunlock(&ev->lock);
    return;
  }

  CHK_ALLOC(cbs = malloc(sizeof(*cbs) * cur));
  memcpy(cbs, ent->cbs, sizeof(*cbs) * cur);
  uv_rwlock_rdunlock(&ev->entries[handle].lock);
  uv_rwlock_rdunlock(&ev->lock);
  vgc_job *jobs = malloc(sizeof(*jobs) * cur);
  CHK_ALLOC(jobs);
  for(uint64_t i = 0; i < cur; i++) {
    // There has got to be a better way to do this
    sbev_tramp_arg *arg = malloc(sizeof(*arg));
    CHK_ALLOC(arg);
    arg->cbs = cbs[i];
    arg->ev_data = ev_data;
    arg->handle = handle;
    jobs[i] = vgc_job_init(sbev_trampoline, arg, FIBER_MID);
  }
  // Eventually we should handle this more gracefully
  if(vgc_schedule_jobs(fiber, jobs, cur, count)) {
    log_error("Error while scheduling jobs from emit");
    exit(-1);
  }
  free(cbs);
  free(jobs);
}

void sbev_kill(sbev_eventcore *ev) {
  log_debug("Kill called");
  uv_rwlock_wrlock(&ev->lock);
  ev->kill_flag = 1;
  uv_rwlock_wrunlock(&ev->lock);
}

void sbev_run_event_once(sbev_eventcore *ev, vgc_fiber fiber) {
  vgc_counter count;
  if(!ev->kill_flag)
    sbev_emit_event(ev, ev->tick, NULL, fiber, &count);
  else
    sbev_emit_event(ev, ev->kill, NULL, fiber, &count);
  fiber = vgc_wait_for_counter(fiber, &count);
}

void sbev_run_event_continous(sbev_eventcore *ev, vgc_fiber fiber) {
  vgc_counter count;
  while(!ev->kill_flag) {
    sbev_emit_event(ev, ev->tick, NULL, fiber, &count);
    fiber = vgc_wait_for_counter(fiber, &count);
  }
  sbev_emit_event(ev, ev->kill, NULL, fiber, &count);
  fiber = vgc_wait_for_counter(fiber, &count);
}

void sbev_start_event(sbev_eventcore *ev, vgc_fiber fiber, int continuous) {
  vgc_counter count;
  sbev_emit_event(ev, ev->start, NULL, fiber, &count);
  fiber = vgc_wait_for_counter(fiber, &count);
  if(continuous)
    sbev_run_event_continous(ev, fiber);
}
