#include "lib_mach_info.h"
#include <mach/message.h>
//#include <pthread/mutex.h>
#include <dispatch/dispatch.h>

extern void detour(struct mach_o_handler *handler_ptr);

mach_msg_return_t (*orig_mach_msg)(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_size,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify) = NULL;

static dispatch_once_t init_mach_msg_once_token;

mach_msg_return_t mach_msg(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_size,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify)
{
	dispatch_once (&init_mach_msg_once_token, ^{
		orig_mach_msg = dlsym(RTLD_NEXT, "mach_msg");
		get_func_ptr_from_lib(mach_msg_destroy, "mach_msg", detour);
	});
	return orig_mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify);
}
