#include <stdnoreturn.h>
#include <uv.h>

#include "sds.h"
#include "uthash.h"
#include "logc/log.h"
#include "sb_event.h"
#include "sb_net.h"
#include "fibers/vgc.h"

#define ERR_CHK(x, str) do { if(x < 0) { log_error(str); return -1;}} while(0)

#define CHK_ALLOC(x) do {                                                     \
  if(!(x)) {                                                                  \
    log_error("Failed to allocate");                                          \
    exit(-1);                                                                 \
  }                                                                           \
} while(0)

noreturn static void run_event(vgc_fiber fiber) {
  sbev_eventcore ev;
  sbnet_netcore net;
  sbnet_settings settings = {
    .addr = "localhost",
    .port = 25565,
    .username = "nickelpro"
  };
  sbev_init_event(&ev);
  sbnet_init_net(&net, &ev, settings);
  sbev_start_event(&ev, fiber, 1);
  exit(0);
}

void log_lock(void *udata, int lock) {
  if(lock)
    uv_mutex_lock(udata);
  else
    uv_mutex_unlock(udata);
}

int main(int argc, char *argv[]) {
  uv_cpu_info_t *info;
  int cpu_count;
  ERR_CHK(uv_cpu_info(&info, &cpu_count), "Error getting CPU info");
  uv_free_cpu_info(info, cpu_count);

  uv_mutex_t log_mux;
  uv_mutex_init(&log_mux);
  log_set_lock(log_lock);
  log_set_udata(&log_mux);

  uv_thread_t *threads;
  CHK_ALLOC(threads = malloc(sizeof(*threads) * cpu_count));
  vgc_scheduler sched;
  vgc_scheduler_init(&sched, 128);
  vgc_job job = vgc_job_init(run_event, NULL, FIBER_HI);
  vgc_enque_job(&sched, job, NULL);
  for(int i = 0; i < cpu_count; i++)
    uv_thread_create(&threads[i], vgc_scheduler_run, &sched);
  vgc_scheduler_run(&sched);
}
