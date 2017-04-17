#include "lib_mach_info.h"
#include <CoreFoundation/CFRunLoop.h>

#define DEBUG_INIT				0x23456770
#define DEBUG_CreateObserver	0x23456780
#define DEBUG_AddObserver		0x23456784
#define DEBUG_RemoveObserver	0x23456788
#define DEBUG_CallObserver		0x2345678c

#if defined(__LP64__)
static CFRunLoopObserverRef (*orig_CFRunLoopObserverCreate)(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context) = NULL;

CFRunLoopObserverRef CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context)
{
	if (orig_CFRunLoopObserverCreate == NULL) {
		orig_CFRunLoopObserverCreate = get_func_ptr_from_lib(CFGetAllocator,"CFRunLoopObserverCreate");
		kdebug_trace(DEBUG_INIT, orig_CFRunLoopObserverCreate, 0, 0, 0, 0);
	}

	if (orig_CFRunLoopObserverCreate)
		return orig_CFRunLoopObserverCreate(allocator, activities, repeats, order, callout, context);

	printf("Fail to Creat RunLoopObserver\n");
	return NULL;	
}
#endif
