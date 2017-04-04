#include "lib_mach_info.h"

#if defined(__LP64__)
static CFRunLoopObserverRef (*orig_CFRunLoopObserverCreate)(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context) = NULL;

CFRunLoopObserverRef CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context)
{
	if (orig_CFRunLoopObserverCreate == NULL) {
		orig_CFRunLoopObserverCreate = get_func_ptr_from_lib(CFGetAllocator,"CFRunLoopObserverCreate");
		detour();
		kdebug_trace(DEBUG_INIT, orig_CFRunLoopObserverCreate, 0, 0, 0, 0);
	}

	if (orig_CFRunLoopObserverCreate)
		return orig_CFRunLoopObserverCreate(allocator, activities, repeats, order, callout, context);

	printf("Fail to Creat RunLoopObserver\n");
	return NULL;	
}
#endif
