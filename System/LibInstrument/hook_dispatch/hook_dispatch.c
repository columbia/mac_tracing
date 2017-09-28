#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

//#if defined(__LP64__)
extern void detour_enqueue(struct mach_o_handler *handler_ptr);
extern void detour_dequeue(struct mach_o_handler *handler_ptr);
extern void detour_blockinvoke(struct mach_o_handler *handler_ptr);

void detour(struct mach_o_handler *handler_ptr)
{
	kdebug_trace(DEBUG_INIT, 0, 0, 0, 0, 0);
	detour_enqueue(handler_ptr);
	detour_dequeue(handler_ptr);
	detour_blockinvoke(handler_ptr);
}

void (*orig_libdispatch_init)(void) = NULL;
void libdispatch_init(void)
{
	if (orig_libdispatch_init == NULL) {
		orig_libdispatch_init = get_func_ptr_from_lib(dispatch_async,
			"libdispatch_init", detour);
	}
	orig_libdispatch_init();
}
//#endif
