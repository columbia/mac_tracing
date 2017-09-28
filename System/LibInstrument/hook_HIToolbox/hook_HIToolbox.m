#include "lib_mach_info.h"
//#import <HIToolbox/CarbonEventsCore.h>
#include <Carbon/Carbon.h>

#define EventRefDebug 0x2bd80104
extern void detour(struct mach_o_handler *handler_ptr);
static bool init = false;

void init_hook()
{
	if (init == false) {
		get_func_ptr_from_lib(GetEventClass, "[IMKClient isClientServerTracing]", detour);
		init = true;
	}
}
