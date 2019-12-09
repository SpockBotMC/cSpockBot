#ifndef SB_EVENT_H
#define SB_EVENT_H

#include <stdint.h>
#include "sds.h"
#include "uthash.h"

typedef void (*sbev_event_cb)(void *cb_data, void *ev_data, uint64_t handle);

typedef struct {
	sbev_event_cb cb;
	void *cb_data;
} sbev_cb_and_data;

typedef struct {
	sds nomen;
	uint64_t handle;
	sbev_cb_and_data *cbs;
	size_t max;
	size_t cur;
	UT_hash_handle hh;
} sbev_event_entry;

typedef struct {
	// Event hashmap and lookup array
	sbev_event_entry *hashmap;
	sbev_event_entry *array;
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

void sbev_init_event(sbev_eventcore *ev);
uint64_t sbev_reg_event(sbev_eventcore *ev, char *nomen);
void sbev_reg_cb(sbev_eventcore *ev, uint64_t handle, sbev_event_cb cb,
                 void *cb_data);
void sbev_reg_event_cb(sbev_eventcore *ev, char *nomen, sbev_event_cb cb,
                       void *cb_data);

void sbev_run_event_once(sbev_eventcore *ev);
void sbev_run_event_continous(sbev_eventcore *ev);
void sbev_start_event(sbev_eventcore *ev, int continuous);
#endif
