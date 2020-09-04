// Originally based on Dimitry Vyukov's implementation
// http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue

#include "fibers/vgc.h"
#include <stdint.h>

// buf must be at least sizeof(vgc_cell)*size
// size must be a power of 2 and >= 2
vgc_ringbuf vgc_ringbuf_init(
  void *buf,
  size_t size
) {
  vgc_ringbuf rb;
  rb.buf = (vgc_cell *) buf;
  rb.bufmask = size - 1;
  for(size_t i = 0; i < size; i++)
    atomic_store_explicit(&rb.buf[i].seq, i, memory_order_relaxed);
  atomic_store_explicit(&rb.head, 0, memory_order_relaxed);
  atomic_store_explicit(&rb.tail, 0, memory_order_relaxed);
  return rb;
}

// buf must be at least sizeof(vgc_cell)*size
// size must be a power of 2 and >= 2
vgc_queue vgc_queue_init(
  void *buf,
  size_t size,
  vgc_scheduler *sched
) {
  vgc_queue q;
  q.rb = vgc_ringbuf_init(buf, size);
  q.sched = sched;
  return q;
}

int vgc_push(vgc_ringbuf *rb, void *data) {
  vgc_cell *cell;
  size_t pos = atomic_load_explicit(&rb->tail, memory_order_relaxed);
  for(;;) {
    cell = &rb->buf[pos & rb->bufmask];
    size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
    intptr_t dif = (intptr_t) seq - (intptr_t) pos;
    if(dif == 0) {
      if((atomic_compare_exchange_weak_explicit(&rb->tail, &pos, pos + 1,
        memory_order_relaxed, memory_order_relaxed))) break;
    } else if(dif < 0) {
      return -1;
    } else {
      pos = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    }
  }
  cell->data = data;
  atomic_store_explicit(&cell->seq, pos + 1, memory_order_release);
  return 0;
}

int vgc_pop(vgc_ringbuf *rb, void **data) {
  vgc_cell *cell;
  size_t pos = atomic_load_explicit(&rb->head, memory_order_relaxed);
  for(;;) {
    cell = &rb->buf[pos & rb->bufmask];
    size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
    intptr_t dif = (intptr_t) seq - (intptr_t) (pos + 1);
    if(dif == 0) {
      if((atomic_compare_exchange_weak_explicit(&rb->head, &pos, pos + 1,
        memory_order_relaxed, memory_order_relaxed))) break;
    } else if(dif < 0) {
      return -1;
    } else {
      pos = atomic_load_explicit(&rb->head, memory_order_relaxed);
    }
  }
  *data = cell->data;
  atomic_store_explicit(
    &cell->seq, pos + rb->bufmask + 1, memory_order_release
  );
  return 0;
}

int vgc_enqueue(vgc_queue *q, void *data) {
  if(vgc_push(&q->rb, data))
    return -1;

  vgc_scheduler *sched = q->sched;
  if(atomic_load_explicit(&sched->is_waiter, memory_order_relaxed)) {
    uv_mutex_lock(&sched->waiter_mux);
    uv_cond_broadcast(&sched->waiter_cond);
    uv_mutex_unlock(&sched->waiter_mux);
  }
  return 0;
}

int vgc_dequeue(vgc_queue *q, void **data) {
  return vgc_pop(&q->rb, data);
}
