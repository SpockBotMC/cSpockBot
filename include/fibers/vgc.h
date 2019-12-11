#ifndef VGC_H
#define VGC_H

#include <uv.h>

#include <stdalign.h>
#include <stdatomic.h>
#include <stdnoreturn.h>

typedef struct vgc_cell_s {
	atomic_size_t seq;
	void *data;
} vgc_cell;

typedef struct vgc_ringbuf_s {
	size_t bufmask;
	vgc_cell *buf;
	#define cacheline_size 64
	alignas(cacheline_size) atomic_size_t head;
	alignas(cacheline_size) atomic_size_t tail;
	#undef cacheline_size
} vgc_ringbuf;

vgc_ringbuf vgc_ringbuf_init(
	void *buf,
	size_t size
);
int vgc_push(vgc_ringbuf *rb, void *data);
int vgc_pop(vgc_ringbuf *rb, void **data);

typedef struct vgc_scheduler_s vgc_scheduler;

typedef struct vgc_queue {
	vgc_ringbuf rb;
	vgc_scheduler *sched;
} vgc_queue;

typedef struct vgc_scheduler_s {
	vgc_queue hi_q;
	vgc_queue mid_q;
	vgc_queue lo_q;
	vgc_ringbuf free_q_pool;
	uv_mutex_t waiter_mux;
	uv_cond_t waiter_cond;
	atomic_bool is_waiter;
} vgc_scheduler;

vgc_queue vgc_queue_init(
	void *buf,
	size_t size,
	vgc_scheduler *sched
);
int vgc_enqueue(vgc_queue *q, void *data);
int vgc_dequeue(vgc_queue *q, void **data);

typedef struct fiber_data_s fiber_data;

typedef struct vgc_fiber_s {
	void *data; //user data
	void *ctx;
	fiber_data *fd;
} vgc_fiber;

#ifndef WAIT_FIBER_LEN
#define WAIT_FIBER_LEN 4
#endif
typedef struct vgc_counter_s {
	atomic_int lock;
	int counter;
	size_t waiters_len;
	vgc_fiber *waiters[WAIT_FIBER_LEN];
} vgc_counter;

typedef void (*vgc_proc)(vgc_fiber fiber);

typedef enum {
	FIBER_LO,
	FIBER_MID,
	FIBER_HI
} fiber_priority;

typedef struct {
	vgc_proc proc;
	void *data;
	fiber_priority priority;
} vgc_job;

typedef struct fiber_data_s {
	int id;
	enum {
		FIBER_START,
		FIBER_RESUME,
		FIBER_RUN,
		FIBER_WAIT,
		FIBER_DONE
	} state;
	fiber_priority priority;
	vgc_scheduler *sched;
	vgc_counter *dependency_count;
	vgc_counter *parent_count;
	void *stack_limit;
	void *stack_base;
} fiber_data;

vgc_counter vgc_counter_init(int count);

vgc_fiber vgc_fiber_assign(vgc_fiber fiber, vgc_proc proc);
vgc_fiber vgc_fiber_init(void *buf, size_t size, fiber_data *fd);
noreturn void vgc_fiber_finish(vgc_fiber fiber);

extern void *vgc_make(void *base, void *limit, vgc_proc proc);
extern vgc_fiber vgc_jump(vgc_fiber fiber);

int vgc_enque_job(vgc_scheduler *sched, vgc_job job, vgc_counter *count);

vgc_job vgc_job_init(vgc_proc proc, void *data, fiber_priority priority);
int vgc_schedule_job(vgc_fiber fiber, vgc_job job, vgc_counter *count);
int vgc_schedule_jobs(vgc_fiber fiber, vgc_job *jobs, int len,
                      vgc_counter *count);

vgc_fiber vgc_wait_for_counter(vgc_fiber fiber, vgc_counter *count);
vgc_fiber vgc_wait_for_counter2(vgc_fiber fiber, vgc_counter *count, int *err);

void vgc_scheduler_init(vgc_scheduler *sched, size_t size);
void vgc_scheduler_run(void *p);

#endif
