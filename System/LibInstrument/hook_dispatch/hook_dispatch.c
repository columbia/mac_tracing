#include "lib_mach_info.h"

static void * handle  = NULL;

#define store_orig_func_ptr(orig_func_ptr, name)\
	handle = dlopen("/usr/lib/system/orig.libdispatch.dylib", RTLD_LAZY);\
	if (handle == NULL) {\
		dlerror();\
		exit(EXIT_FAILURE);\
	}\
	dlerror();\
	orig_func_ptr = dlsym(handle, name);\
	if (dlerror() != NULL){\
		exit(EXIT_FAILURE);\
	}\
	dlclose(handle);\
	handle = NULL;

//extern void detour_enqueue(struct hack_handler *hack_handler_ptr);
//extern void detour_dequeue(struct hack_handler *hack_handler_ptr);
//extern void detour_blockinvoke(struct hack_handler *hack_handler_ptr);

void (*orig_libdispatch_init)(void) = NULL;
void libdispatch_init(void)
{
	if (orig_libdispatch_init == NULL) {
		#if defined(__LP64__)
		struct hack_handler hack_handler_info;
		bool ret = false;
		
		kdebug_trace(0x210a0fac, 0, 0, 0, 0, 0);
		memset(&hack_handler_info, 0, sizeof(struct hack_handler));
		ret = prepare_detour(&hack_handler_info);
		if (ret) {
			detour_enqueue(&hack_handler_info);
			detour_dequeue(&hack_handler_info);
			detour_blockinvoke(&hack_handler_info);
		}
		#endif

		store_orig_func_ptr(orig_libdispatch_init, "libdispatch_init");

		#if defined(__LP64__)
		if (hack_handler_info.file_address != NULL)
			free(hack_handler_info.file_address);
		#endif
	}
	orig_libdispatch_init();
}
