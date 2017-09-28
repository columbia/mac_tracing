#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include "symbolicator.h"
#define DEBUG	1
#define SET_BR	1

#define EXE 0
#define WO	1
#define RW	3

#define Byte	0
#define Word	1
#define DWord	3	

kern_return_t set_breakpoint(mach_port_t thread, void *addr, int dr_idx, uint32_t rw_option, uint32_t len)
{
	struct x86_debug_state dr;
	mach_msg_type_number_t dr_count = x86_DEBUG_STATE_COUNT;
	kern_return_t rc = thread_get_state(thread, x86_DEBUG_STATE, (thread_state_t)&dr, &dr_count);
	if (rc != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read debug state in thread %s\n", mach_error_string(rc));
#endif
		return rc;
	}

	switch (dr_idx) {
#ifdef __x86_64__
		dr.uds.ds64.__dr7 |= (3 << 8);
		case 0:
			dr.uds.ds64.__dr0 = (uint64_t)addr;
			dr.uds.ds64.__dr7 |= (len << 18) | (rw_option << 16) | 1;
			break;
		case 1:
			dr.uds.ds64.__dr1 = (uint64_t)addr;
			dr.uds.ds64.__dr7 |= (len << 22) | (rw_option << 20) | (1 << 2);
			break;
		case 2:
			dr.uds.ds64.__dr2 = (uint64_t)addr;
			dr.uds.ds64.__dr7 |= (len << 26) | (rw_option << 24) | (1 << 4);
			break;
		case 3:
			dr.uds.ds64.__dr3 = (uint64_t)addr;
			dr.uds.ds64.__dr7 |= (len << 30) | (rw_option << 28) | (1 << 6);
			break;
		default:
			break;
#elif __i386__
		dr.uds.ds32.__dr7 |= (3 << 8);
		case 0:
			dr.uds.ds32.__dr0 = (uint32_t)addr;
			dr.uds.ds32.__dr7 |= (len << 18) | (rw_option << 16) | 1;
			break;
		case 1:
			dr.uds.ds32.__dr1 = (uint32_t)addr;
			dr.uds.ds32.__dr7 |= (len << 22) | (rw_option << 20) | (1 << 2);
			break;
		case 2:
			dr.uds.ds32.__dr2 = (uint32_t)addr;
			dr.uds.ds32.__dr7 |= (len << 26) | (rw_option << 24) | (1 << 4);
			break;
		case 3:
			dr.uds.ds32.__dr3 = (uint32_t)addr;
			dr.uds.ds32.__dr7 |= (len << 30) | (rw_option << 28) | (1 << 6);
			break;
		default:
			break;
#endif
	}

	return thread_set_state(thread, x86_DEBUG_STATE, (thread_state_t)&dr, dr_count);
}

kern_return_t clear_breakpoint(mach_port_t thread, int dr_idx)
{
	struct x86_debug_state dr;
	mach_msg_type_number_t dr_count = x86_DEBUG_STATE_COUNT;
	kern_return_t rc = thread_get_state(thread, x86_DEBUG_STATE, (thread_state_t)&dr, &dr_count);
	if (rc != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read debug state in thread %s\n", mach_error_string(rc));
#endif
		return rc;
	}

	switch (dr_idx) {
#ifdef __x86_64__
		case 0:
			dr.uds.ds64.__dr0 = (uint64_t)0;
			dr.uds.ds64.__dr7 &= ~((0xf << 16) | 1);
			break;
		case 1:
			dr.uds.ds64.__dr1 = (uint64_t)0;
			dr.uds.ds64.__dr7 &= ~((0xf << 20) | (1 << 2));
			break;
		case 2:
			dr.uds.ds64.__dr2 = (uint64_t)0;
			dr.uds.ds64.__dr7 &= ~((0xf << 24) | (1 << 4));
			break;
		case 3:
			dr.uds.ds64.__dr3 = (uint64_t)0;
			dr.uds.ds64.__dr7 &= ~((0xf << 28) | (1 << 6));
			break;
		default:
			break;
#elif __i386__
		case 0:
			dr.uds.ds32.__dr0 = (uint32_t)0;
			dr.uds.ds32.__dr7 &= ~((0xf << 16) | 1);
			break;
		case 1:
			dr.uds.ds32.__dr1 = (uint32_t)0;
			dr.uds.ds32.__dr7 &= ~((0xf << 20) | (1 << 2));
			break;
		case 2:
			dr.uds.ds32.__dr2 = (uint32_t)0;
			dr.uds.ds32.__dr7 &= ~((0xf << 24) | (1 << 4));
			break;
		case 3:
			dr.uds.ds32.__dr3 = (uint32_t)0;
			dr.uds.ds32.__dr7 &= ~((0xf << 28) | (1 << 6));
			break;
		default:
			break;
#endif
	}

	return thread_set_state(thread, x86_DEBUG_STATE, (thread_state_t)&dr, dr_count);
}

static kern_return_t set_breakpoints_for_all_threads(mach_port_t target_task, void *addr, int dr_idx, uint32_t option, uint32_t len)
{
	kern_return_t kret;
	thread_act_port_array_t threadList;
	mach_msg_type_number_t threadCount;
	
	kret = task_threads(target_task, &threadList, &threadCount);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read threads %s\n", mach_error_string(kret));
#endif
		return kret;
	}

	for (int i = 0; i < threadCount; i++) {
		kret = set_breakpoint(threadList[i], addr, dr_idx, option, len);
		if (kret != KERN_SUCCESS) {
#if DEBUG
			printf("unable to set breakpoint for thread %s\n", mach_error_string(kret));
#endif
			return kret;
		}
	}
	return kret;
}

static kern_return_t clear_breakpoints_for_all_threads(mach_port_t target_task, int dr_idx)
{
	kern_return_t kret;
	thread_act_port_array_t threadList;
	mach_msg_type_number_t threadCount;
	
	kret = task_threads(target_task, &threadList, &threadCount);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read threads %s\n", mach_error_string(kret));
#endif
		return kret;
	}

	for (int i = 0; i < threadCount; i++) {
		kret = clear_breakpoint(threadList[i], dr_idx);
		if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("unable to set breakpoint for thread %s\n", mach_error_string(kret));
#endif
			return kret;
		}
	}
	return kret;
}

int set_br(pid_t pid, const char *sympath, const char *symbol, int reg_index, int watch_option, int watch_len) 
{
	mach_port_t target_task = MACH_PORT_NULL;
	struct mach_o_handler lib_handler;
	void *addr;
	int ret;
	memset(&lib_handler, 0, sizeof(struct mach_o_handler));
	addr = my_dyld(pid, sympath, symbol, &lib_handler);
	if (lib_handler.symbol_arrays != NULL)
		free(lib_handler.symbol_arrays);
	if (lib_handler.strings != NULL)
		free(lib_handler.strings);
#if DEBUG
	printf("A hardware breakpoint register watches address : %p\n", addr);
#endif

	ret = task_for_pid(mach_task_self(), pid, &target_task);
	if (ret != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read task %s\n", mach_error_string(ret));
#endif
		return ret;
	}
	
	return set_breakpoints_for_all_threads(target_task, addr, reg_index, watch_option, watch_len);
}

int clear_br(pid_t pid, int reg_index)
{
	mach_port_t target_task = MACH_PORT_NULL;
	int ret;

	ret = task_for_pid(mach_task_self(), pid, &target_task);
	if (ret != KERN_SUCCESS) {
#if DEBUG
		printf("unable to read task %s\n", mach_error_string(ret));
#endif
		return ret;
	}
	return clear_breakpoints_for_all_threads(target_task, reg_index);
}

#if SET_BR
int main(int argc, char *argv[])
{
	int pid;
	char sympath[1024] = "";
	char symbol[80] = "";
	int reg_index = 0;
	int watch_option = RW;
	int watch_len;

	if (argc < 7) {
		printf("usage: %s [pid] [symbol_path] [symbol] [reg_index] [watch_option<RW|WO|EXE>] [watch_len]\n", argv[0]);
		return -1;
	}

	pid = atoi(argv[1]);
	strcpy(sympath, argv[2]);
	strcpy(symbol, argv[3]);
	reg_index = atoi(argv[4]);
	
	if (strncmp(argv[5], "RW", 2) == 0)
		watch_option = RW;
	else if (strncmp(argv[5], "WO", 2) == 0)
		watch_option = WO;
	else if (strncmp(argv[5], "EXE", 3) == 0)
		watch_option = EXE;
	else {
		printf("invalid watching option %s\n", argv[5]);
		exit(EXIT_FAILURE);
	}
	
	if (strcmp(argv[6], "Byte") == 0)
		watch_len = Byte;
	else if (strcmp(argv[6], "Word") == 0)
		watch_len = Word;
	else if (strcmp(argv[6], "DWord") == 0)
		watch_len = DWord;
	else {
		printf("invalid watching length %s\n", argv[6]);
		exit(EXIT_FAILURE);
	}
	
	printf("%s %d %s %s %d ", argv[0], pid, sympath, symbol, reg_index);
	switch (watch_option) {
		case EXE:
			printf("execution");
			break;
		case WO:
			printf("write");
			break;
		case RW:
			printf("read_or_write");
			break;
		default:
			break;
	}
	printf(" %d\n", watch_len);

	return set_br(pid, sympath, symbol, reg_index, watch_option, watch_len);
}
#else
int main(int argc, char *argv[])
{
	int pid;
	int reg_index = 0;

	if (argc < 3) {
		printf("usage: %s [pid] [reg_index]\n", argv[0]);
		return -1;
	}

	pid = atoi(argv[1]);
	reg_index = atoi(argv[2]);
	return clear_br(pid, reg_index);
}
#endif
