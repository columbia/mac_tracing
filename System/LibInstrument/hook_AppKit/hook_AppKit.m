#include "lib_mach_info.h"
#import <AppKit/AppKit.h>
#import <objc/runtime.h>

//#define DEBUG_NSEvent 0x23456790

extern void detour(struct mach_o_handler *handler_ptr);

static bool init = false;
void init_hook()
{
	if (init == false) {
		get_func_ptr_from_lib(NSApplicationLoad, "[NSApplication run]", detour);
		init = true;
	}
}
