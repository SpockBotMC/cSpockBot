#include <stdint.h>
#include "sds.h"
#include "uthash.h"
#include "logc/log.h"

#include "sb_event.h"

#define CHK_ALLOC(x) do {                                                     \
	if(!(x)) {                                                                  \
		log_error("Failed to allocate");                                          \
		exit(-1);                                                                 \
	}                                                                           \
} while(0)

void sbev_init_event(sbev_eventcore *ev) {
	ev->hashmap = NULL;
	ev->max = 20; // Chosen by fair dice roll
	ev->cur = 0;
	CHK_ALLOC(ev->array = malloc(sizeof(*ev->array) * ev->max));
	ev->kill_flag = 0;
	ev->start = sbev_reg_event(ev, "start");
	ev->tick = sbev_reg_event(ev, "tick");
	ev->kill = sbev_reg_event(ev, "kill");
}

void sbev_init_event_entry(sbev_event_entry *ent, char *nomen, uint64_t handle) {
	CHK_ALLOC(ent->nomen = sdsnew(nomen));
	ent->handle = handle;
	ent->max = 20; // These dice are rigged
	ent->cur = 0;
	CHK_ALLOC(ent->cbs = malloc(sizeof(*ent->cbs) * ent->max));
}

uint64_t sbev_reg_event(sbev_eventcore *ev, char *nomen) {
	sbev_event_entry *find;
	HASH_FIND_STR(ev->hashmap, nomen, find);
	if(find) {
		return find->handle;
	}

	if(ev->cur == ev->max) {
		ev->max *= 2;
		sbev_event_entry *p = realloc(ev->array, sizeof(*ev->array) * ev->max);
		CHK_ALLOC(p);
		if(ev->array != p) {
			ev->array = p;
			HASH_CLEAR(hh, ev->hashmap);
			for(uint64_t i = 0; i < ev->cur; i++) {
				HASH_ADD_KEYPTR(hh, ev->hashmap, p[i].nomen, sdslen(p[i].nomen), &p[i]);
			}
		}
	}
	uint64_t handle = ev->cur++;
	sbev_event_entry *ent = &ev->array[handle];
	sbev_init_event_entry(ent, nomen, handle);
	HASH_ADD_KEYPTR(hh, ev->hashmap, ent->nomen, sdslen(ent->nomen), ent);
	return handle;
}

void sbev_reg_cb(sbev_eventcore *ev, uint64_t handle, sbev_event_cb cb,
                 void *cb_data) {
	sbev_event_entry *ent = &ev->array[handle];
	if(ent->cur == ent->max) {
		ent->max *= 2;
		sbev_cb_and_data *p = realloc(ent->cbs, sizeof(*ent->cbs) * ent->max);
		CHK_ALLOC(p);
		ent->cbs = p;
	}
	ent->cbs[ent->cur].cb = cb;
	ent->cbs[ent->cur].cb_data = cb_data;
	ent->cur++;
}

void sbev_reg_event_cb(sbev_eventcore *ev, char *nomen, sbev_event_cb cb,
                       void *cb_data) {
	uint64_t handle = sbev_reg_event(ev, nomen);
	sbev_reg_cb(ev, handle, cb, cb_data);
}

void sbev_emit_event(sbev_eventcore *ev, uint64_t handle, void *ev_data) {
	for(uint64_t i = 0; i < ev->array[handle].cur; i++) {
		sbev_cb_and_data tmp = ev->array[handle].cbs[i];
		tmp.cb(tmp.cb_data, ev_data, handle);
	}
}

void sbev_run_event_once(sbev_eventcore *ev) {
	if(!ev->kill_flag) {
		sbev_emit_event(ev, ev->tick, NULL);
		return;
	}
	sbev_emit_event(ev, ev->kill, NULL);
}

void sbev_run_event_continous(sbev_eventcore *ev) {
	while(!ev->kill_flag)
		sbev_emit_event(ev, ev->tick, NULL);
	sbev_emit_event(ev, ev->kill, NULL);
}

void sbev_start_event(sbev_eventcore *ev, int continuous) {
	sbev_emit_event(ev, ev->start, NULL);
	if(continuous)
		sbev_run_event_continous(ev);
}
