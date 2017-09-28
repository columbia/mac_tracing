#include "lib_mach_info.h"
//#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>
#include <signal.h>
#include <pthread.h>

#define HWBR_DEBUG	0x2bd80120
#define current_thread pthread_mach_thread_np(pthread_self())

#define Byte	0
#define Word	1
#define DWord	3	

#define EXE 0
#define WO	1
#define RW	3

extern void detour(struct mach_o_handler * handler_ptr);
static CFMachPortRef (*orig_CFMachPortCreate)(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo) = NULL;
static void (*appkit_hook_entry)() = NULL;
static void (*hitoolbox_hook_entry)() = NULL;
static void (*ca_hook_entry)() = NULL;
//static void (*cgs_hook_entry)() = NULL;

static uint32_t read_regval(void *addr, int rw, int len)
{
	if (rw != WO && rw != RW)
		return -1;

	switch (len) {
		case Byte:
			return *(uint8_t *)addr;
		case Word:
			return *(uint16_t *)addr;
		case DWord:
			return *(uint32_t *)addr;
		defaule:
			break;
	}
	return -1;
}

static kern_return_t read_monitored_addr(uint64_t *addrs, uint32_t *array, int size)
{
	struct x86_debug_state dr;
	void *monitored_addr;
	mach_msg_type_number_t dr_count = x86_DEBUG_STATE_COUNT;
	kern_return_t rc = thread_get_state(current_thread, x86_DEBUG_STATE, &dr, &dr_count);
	if (rc != KERN_SUCCESS)
		return rc;
	if (size != 4)
		return KERN_ABORTED;

#ifdef __x86_64__
	monitored_addr = dr.uds.ds64.__dr7 & 1ULL ? dr.uds.ds64.__dr0 : 0 ;
	if (monitored_addr != 0)
		array[0] = read_regval(monitored_addr, (dr.uds.ds64.__dr7 & (3UL << 16)) >> 16, (dr.uds.ds64.__dr7 & (3UL << 18)) >> 18);
	addrs[0] = (uint64_t)monitored_addr;

	monitored_addr = dr.uds.ds64.__dr7 & (1ULL << 2) ? dr.uds.ds64.__dr1 : 0 ;
	if (monitored_addr != 0)
		array[1] = read_regval(monitored_addr, (dr.uds.ds64.__dr7 & (3UL << 20)) >> 20, (dr.uds.ds64.__dr7 & (3UL << 22)) >> 22);
	addrs[1] = (uint64_t)monitored_addr;


	monitored_addr = dr.uds.ds64.__dr7 & (1ULL << 4) ? dr.uds.ds64.__dr2 : 0 ;
	if (monitored_addr != 0)
		array[2] = read_regval(monitored_addr, (dr.uds.ds64.__dr7 & (3UL << 24)) >> 24, (dr.uds.ds64.__dr7 & (3UL << 26)) >> 26);
	addrs[2] = (uint64_t)monitored_addr;


	monitored_addr = dr.uds.ds64.__dr7 & (1ULL << 6) ? dr.uds.ds64.__dr3 : 0 ;
	if (monitored_addr != 0)
		array[3] = read_regval(monitored_addr, (dr.uds.ds64.__dr7 & (3UL << 28)) >> 28, (dr.uds.ds64.__dr7 & (3UL << 30)) >> 30);
	addrs[3] = (uint64_t)monitored_addr;


#elif __i386__
	monitored_addr = dr.uds.ds32.__dr7 & 1ULL ? dr.uds.ds32.__dr0 : 0 ;
	if (monitored_addr != 0)
		array[0] = read_regval(monitored_addr, (dr.uds.ds32.__dr7 & (3UL << 16)) >> 16, (dr.uds.ds32.__dr7 & (3UL << 18)) >> 18);
	addrs[0] = (uint64_t)monitored_addr;


	monitored_addr = dr.uds.ds32.__dr7 & (1ULL << 2) ? dr.uds.ds32.__dr1 : 0 ;
	if (monitored_addr != 0)
		array[1] = read_regval(monitored_addr, (dr.uds.ds32.__dr7 & (3UL << 20)) >> 20, (dr.uds.ds32.__dr7 & (3UL << 22)) >> 22);
	addrs[1] = (uint64_t)monitored_addr;


	monitored_addr = dr.uds.ds32.__dr7 & (1ULL << 4) ? dr.uds.ds32.__dr2 : 0 ;
	if (monitored_addr != 0)
		array[2] = read_regval(monitored_addr, (dr.uds.ds32.__dr7 & (3UL << 24)) >> 24, (dr.uds.ds32.__dr7 & (3UL << 26)) >> 26);
	addrs[2] = (uint64_t)monitored_addr;


	monitored_addr = dr.uds.ds32.__dr7 & (1ULL << 6) ? dr.uds.ds32.__dr3 : 0 ;
	if (monitored_addr != 0)
		array[3] = read_regval(monitored_addr, (dr.uds.ds32.__dr7 & (3UL << 28)) >> 28, (dr.uds.ds32.__dr7 & (3UL << 30)) >> 30);
	addrs[3] = (uint64_t)monitored_addr;

#endif

	return KERN_SUCCESS;
}

static void hwbr_handler(int sig, siginfo_t *info, void *ucontext)
{
	if (info->si_code == TRAP_BRKPT) {
		#if 0 /*si_addr is the caller_adddr*/
		ucontext_t *uc = (ucontext_t *)ucontext;
		void *caller_addr;
#if __x86_64__
		caller_addr = uc->uc_mcontext->__ss.__rip;
#elif __i386__
		caller_addr = uc->uc_mcontext->__ss.__eip;
#endif
		#endif
		// get the addr in break registers that set in current thread
		// read the value and put them in the tracing points
		uint64_t monitored_addr[4] = {0};
		uint32_t monitored_val[4] = {-1, -1, -1, -1};
		if (read_monitored_addr(monitored_addr, monitored_val, 4) == KERN_SUCCESS) {
			kdebug_trace(HWBR_DEBUG, (uint64_t)(info->si_addr),
					//(uint64_t)caller_addr, 
					(((uint64_t)monitored_val[0]) << 32) | monitored_val[1],
					(((uint64_t)monitored_val[2]) << 32) | monitored_val[3],
					monitored_addr[0],
					0);
			kdebug_trace(HWBR_DEBUG, (uint64_t)(info->si_addr),
					monitored_addr[1],
					monitored_addr[2],
					monitored_addr[3],
					0);
		} else {
			// fail to read value
			kdebug_trace(HWBR_DEBUG, (uint64_t)(info->si_addr),
					0ULL, 0ULL, 0ULL, 0);
		}
		back_trace((uint64_t)(info->si_addr));
	} else {
		// not a breakpoint trap
		kdebug_trace(HWBR_DEBUG, 0ULL, 0ULL, 0ULL, 0ULL, 0);
	}
}

static void install_sig(void *breakpoint_handler)
{
	struct sigaction action;
	action.sa_sigaction = hwbr_handler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGTRAP, &action, NULL);
}

CFMachPortRef CFMachPortCreate(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo)
{
	if (orig_CFMachPortCreate == NULL) {
		orig_CFMachPortCreate = get_func_ptr_from_lib(CFGetAllocator, "CFMachPortCreate", detour);
		//kdebug_trace(DEBUG_INIT, (uint64_t)orig_CFMachPortCreate, 0ULL, 0ULL, 0ULL, 0);

		void * handle = dlopen("/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit", RTLD_LAZY);
		if (handle != NULL) {
			appkit_hook_entry = dlsym(handle, "init_hook");
			if (appkit_hook_entry != NULL)
				appkit_hook_entry();
			dlclose(handle);
		}

		handle = dlopen("/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/HIToolbox", RTLD_LAZY);
		if (handle != NULL) {
			hitoolbox_hook_entry = dlsym(handle, "init_hook");
			if (hitoolbox_hook_entry != NULL)
				hitoolbox_hook_entry();
			dlclose(handle);
		}

		handle = dlopen("/System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore", RTLD_LAZY);
		if (handle != NULL) {
			ca_hook_entry = dlsym(handle, "init_hook");
			if (ca_hook_entry != NULL)
				ca_hook_entry();
			dlclose(handle);
		}

		install_sig(hwbr_handler);
	}

	if (orig_CFMachPortCreate)
		return orig_CFMachPortCreate(allocator, callout, context, shouldFreeInfo);
	return NULL;
}
