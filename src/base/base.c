#include "base.h"

int csb_init_plugin(csb_base *plg) {
  return uv_rwlock_init(&plg->lock);
}

void csb_rdlock_plugin(csb_base *plg) {
  uv_rwlock_rdlock(&plg->lock);
}

void csb_rdunlock_plugin(csb_base *plg) {
  uv_rwlock_rdunlock(&plg->lock);
}

void csb_wrlock_plugin(csb_base *plg) {
  uv_rwlock_wrlock(&plg->lock);
}

void csb_wrunlock_plugin(csb_base *plg) {
  uv_rwlock_wrunlock(&plg->lock);
}
