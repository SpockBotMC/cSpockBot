#ifndef CSB_BASE_H
#define CSB_BASE_H

#include <uv.h>

typedef struct csb_base_s {
  uv_rwlock_t lock;
} csb_base;

#define CSB_PLUGIN_BASE csb_base plugin_base;

int csb_init_plugin(csb_base *plg);

void csb_rdlock_plugin(csb_base *plg);
void csb_rdunlock_plugin(csb_base *plg);
void csb_wrlock_plugin(csb_base *plg);
void csb_wrunlock_plugin(csb_base *plg);

#endif
