#include "lib_mach_info.h"
//#import <QuartzCore/CALayer.h>

extern void detour(struct mach_o_handler *handler_ptr);
extern void CARenderOGLRenderDisplay(void *);

static bool init = false;
void init_hook()
{
	if (init == false) {
		get_func_ptr_from_lib(CARenderOGLRenderDisplay, "[CALayer needsDisplayForKey:]", detour);
		init = true;
	}
}
