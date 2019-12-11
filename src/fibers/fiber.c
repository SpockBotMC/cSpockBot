#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "fibers/vgc.h"

vgc_fiber vgc_fiber_init(void *buf, size_t size, fiber_data *fd) {
	fd->stack_base = (void *) (((uintptr_t) buf + (size - 1)) & ~(uintptr_t) 15);
	fd->stack_limit = buf;
	fd->state = FIBER_START;
	fd->parent_count = NULL;
	fd->dependency_count = NULL;
	vgc_fiber fiber;
	fiber.fd = fd;
	return fiber;
}

vgc_fiber vgc_fiber_assign(vgc_fiber fiber, vgc_proc proc) {
	fiber.ctx = vgc_make(
		fiber.fd->stack_base,
		fiber.fd->stack_limit,
		proc
	);
	return fiber;
}

noreturn void vgc_fiber_finish(vgc_fiber fiber) {
	fiber.fd->state = FIBER_DONE;
	vgc_jump(fiber);
	// The compiler doesn't believe us about noreturn unless we enforce it with
	// a call to exit
	exit(0);
}
