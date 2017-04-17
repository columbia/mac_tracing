#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#if defined(__LP64__)
extern void detour_enqueue(struct hack_handler *hack_handler_ptr);
extern void detour_dequeue(struct hack_handler *hack_handler_ptr);
extern void detour_blockinvoke(struct hack_handler *hack_handler_ptr);

void detour(struct hack_handler *hack_handler_ptr)
{
	kdebug_trace(DEBUG_INIT, 0, 0, 0, 0, 0);
	detour_enqueue(hack_handler_ptr);
	detour_dequeue(hack_handler_ptr);
	detour_blockinvoke(hack_handler_ptr);
}

void (*orig_libdispatch_init)(void) = NULL;
void libdispatch_init(void)
{
	if (orig_libdispatch_init == NULL) {
		orig_libdispatch_init = get_func_ptr_from_lib(dispatch_async, "libdispatch_init");
	}
	orig_libdispatch_init();
}
#endif
