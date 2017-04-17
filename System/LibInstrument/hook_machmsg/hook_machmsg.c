//#include <execinfo.h>
#include "lib_mach_info.h"
#include <mach/message.h>

#if defined(__LP64__)
mach_msg_return_t (*orig_mach_msg)(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_size,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify) = NULL;

mach_msg_return_t mach_msg(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_size,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify)

{
	if (orig_mach_msg == NULL) {
		orig_mach_msg = get_func_ptr_from_lib(mach_msg_destroy, "mach_msg");
		//dlsym(RTLD_NEXT, "mach_msg");
	}
	return orig_mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify);
}
#endif
