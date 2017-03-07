#include <execinfo.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <mach/message.h>

extern void detour();

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
		orig_mach_msg = dlsym(RTLD_NEXT, "mach_msg");
		detour();
	}
	return orig_mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify);
}

/*
mach_msg_return_t (*orig_mach_msg_trap)(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_limit,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify,
					mach_msg_header_t *rcv_msg,
					mach_msg_size_t rcv_scatter_size);

mach_msg_return_t mach_msg_trap(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_limit,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify,
					mach_msg_header_t *rcv_msg,
					mach_msg_size_t rcv_scatter_size) 

mach_msg_return_t (*orig_mach_msg_overwrite)(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_limit,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify,
					mach_msg_header_t *rcv_msg,
					mach_msg_size_t rcv_scatter_size);

mach_msg_return_t mach_msg_overwrite(
					mach_msg_header_t *msg,
					mach_msg_option_t option,
					mach_msg_size_t send_size,
					mach_msg_size_t rcv_limit,
					mach_port_name_t rcv_name,
					mach_msg_timeout_t timeout,
					mach_port_name_t notify,
					mach_msg_header_t *rcv_msg,
					mach_msg_size_t rcv_scatter_size) 
*/
