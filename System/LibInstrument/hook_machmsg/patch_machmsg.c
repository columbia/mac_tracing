#include "lib_mach_info.h"

#define BACK_TRACE_BUFFER 17
#define MSG_PATHINFO 	0x29000090
#define MSG_BACKTRACE   0x29000094

#if defined(__LP64__)

static void trace_string(uint64_t tag, const char *str, int len)
{       
 	for (int cur = 0; cur < len; cur += 24) {
		int dump = len - cur > 24 ? 24 : len - cur;
 		uint64_t content[3] = {0};
		memcpy((void *)content, (const void *)(str + cur), dump);
 		kdebug_trace(MSG_PATHINFO, tag, content[0], content[1], content[2], 0);
	}
}

static void dump_libinfo_for_curproc(void)
{
	pid_t pid = getpid();
	if (pid <= 0)
		return;

	char proc_path[2048] = {0};
	int ret = proc_pidpath(pid, proc_path, 4096);

	if (ret <= 0 || strstr(proc_path, "/bin/")
		|| strstr(proc_path, "/usr/libexec/xpcproxy"))
		return;

	#ifdef DUMP_LIBINFO_TO_FILE
	bool write_out = false;
	FILE *fp_bout;
	char outfile_path[64] = "/tmp/";

	if (strstr(proc_path, "WindowServer")) {
		sprintf(outfile_path + 5, "%d_libinfo.log", pid);
		if (!(fp_bout = fopen(outfile_path, "a"))) {
			strcat(outfile_path, ".alt");
			if (!(fp_bout = fopen(outfile_path, "a")))
				write_out = false;
			else 
				write_out = true;
		} else {
			write_out = true;
		}
	}
	#endif

	kdebug_trace(MSG_PATHINFO, 0, 0, 0, 0, 0);

	/* get the list of dynamic libraries
	 */
	task_dyld_info_data_t info;
	mach_msg_type_number_t size = TASK_DYLD_INFO_COUNT;
	struct dyld_all_image_infos * all_image_infos;
	kern_return_t kret = task_info(mach_task_self(), TASK_DYLD_INFO, (void*)&info, &size);

	if (kret == KERN_SUCCESS) {
		all_image_infos = (struct dyld_all_image_infos *)(uintptr_t)info.all_image_info_addr;
		for (const struct dyld_image_info* p = all_image_infos->infoArray;
				p < &all_image_infos->infoArray[all_image_infos->infoArrayCount]; ++p) {
			trace_string((uint64_t)(p->imageLoadAddress), p->imageFilePath, (int)strlen(p->imageFilePath));
	#ifdef DUMP_LIBINFO_TO_FILE
			if (write_out) {
				char offset_str[32];
				sprintf(offset_str, "%llx %llx ", (uint64_t)(p->imageLoadAddress), (uint64_t)(p->imageFilePath));
				fwrite(offset_str, 1, strlen(offset_str), fp_bout);
				fwrite(p->imageFilePath, 1, strlen(p->imageFilePath), fp_bout);
				fwrite("\n", sizeof(char), 1, fp_bout);
			}
	#endif
		}
	}

	#ifdef DUMP_LIBINFO_TO_FILE
	if (write_out)
		fclose(fp_bout);
	#endif

	kdebug_trace(MSG_PATHINFO, 1, 1, 1, 1, 1);
}

static uint32_t back_trace(void **bt, uint32_t max)
{
	uint32_t frame, frame_index = 0;
	vm_offset_t stackptr, stackptr_prev, raddr;
	__asm__ volatile("movq %%rbp, %0" : "=m"(stackptr));
	/*
	while (frame_index < 2) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *)stackptr_prev);
		frame_index++;
	}
	*/
	for(frame_index = 0; frame_index < max; frame_index++) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *) stackptr_prev);
		if (stackptr < stackptr_prev)
			break;
		raddr = *((vm_offset_t *)(stackptr + 8));
		if (raddr < 4096)
			break;
		bt[frame_index] = (void*) raddr;
	}
	frame = frame_index;
	for (; frame_index < max; frame_index++)
		bt[frame_index] = (void*)0;
	return frame;
}

#define save_registers { \
	asm volatile("pushq %%rax\n"\
		"pushq %%rbx\n"\
		"pushq %%rdi\n"\
		"pushq %%rsi\n"\
		"pushq %%rdx\n"\
		"pushq %%rcx\n"\
		"pushq %%r8\n"\
		"pushq %%r9\n"\
		"pushq %%r10"\
		::);\
	}

#define restore_registers { \
	asm volatile("popq %%r10\n"\
		"popq %%r9\n"\
		"popq %%r8\n"\
		"popq %%rcx\n"\
		"popq %%rdx\n"\
		"popq %%rsi\n"\
		"popq %%rdi\n"\
		"popq %%rbx\n"\
		"popq %%rax"\
		::);\
	}

bool prepare_detour(struct hack_handler *hack_handler_ptr);
extern void detour_function(struct hack_handler * hack_handler_ptr,
					 char const * sym,
					 void * shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes);
/*
 * 000000000001037c    55                  pushq   %rbp
 * 000000000001037d    4889e5              movq    %rsp, %rbp
 * 0000000000010380    4157                pushq   %r15
 * 0000000000010382    4156                pushq   %r14
 * 0000000000010384    4155                pushq   %r13
 * 0000000000010386    4154                pushq   %r12
 * 0000000000010388    53                  pushq   %rbx
 * 0000000000010389    4883ec18            subq    $0x18, %rsp
 *-> 000000000001038d    4489cb              movl    %r9d, %ebx
 *-> 0000000000010390    4589c7              movl    %r8d, %r15d
 */

void shell_mach_msg(uint64_t msg_addr)
{
	save_registers;
 	void *callstack[BACK_TRACE_BUFFER] = {(void *)0};
	int frames = back_trace(callstack, BACK_TRACE_BUFFER);

	kdebug_trace(MSG_BACKTRACE, msg_addr, frames, callstack[0], callstack[1], 0);

 	for (int i = 2; i < frames && i + 2 < BACK_TRACE_BUFFER; i += 3) {
		if (callstack[i] == (void*)0)
			break;
		kdebug_trace(MSG_BACKTRACE, msg_addr, callstack[i], callstack[i + 1], callstack[i + 2], 0);
	}
	restore_registers;
	//simulation
	asm volatile("movl %%r9d, %%ebx\n"
		"movl %%r8d, %%r15d"
		::);
}

static void detour_mach_msg(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "mach_msg", shell_mach_msg, 17, 6);
}

/*_mach_msg_overwrite:                                                                                                                        
 * 0000000000010480    55                  pushq   %rbp
 * 0000000000010481    4889e5              movq    %rsp, %rbp
 * 0000000000010484    4157                pushq   %r15
 * 0000000000010486    4156                pushq   %r14
 * 0000000000010488    4155                pushq   %r13
 * 000000000001048a    4154                pushq   %r12
 * 000000000001048c    53                  pushq   %rbx
 * 000000000001048d    4883ec38            subq    $0x38, %rsp
 * -> 0000000000010491    4489cb              movl    %r9d, %ebx
 * -> 0000000000010494    4589c7              movl    %r8d, %r15d
 */
void shell_mach_msg_overwrite(uint64_t msg_addr)
{
	save_registers;
 	void *callstack[BACK_TRACE_BUFFER] = {(void *)0};
	int frames = back_trace(callstack, BACK_TRACE_BUFFER);

	kdebug_trace(MSG_BACKTRACE, msg_addr, frames, callstack[0], callstack[1], 0);
 	for (int i = 2; i < frames && i + 2 < BACK_TRACE_BUFFER; i += 3) {
		if (callstack[i] == (void*)0)
			break;
		kdebug_trace(MSG_BACKTRACE, msg_addr, callstack[i], callstack[i + 1], callstack[i + 2], 0);
	}

	restore_registers;
	//simulation
	asm volatile("movl %%r9d, %%ebx\n"
		"movl %%r8d, %%r15d"
		::);
}

static void detour_mach_msg_overwrite(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "mach_msg_overwrite", shell_mach_msg_overwrite, 17, 6);
}

#endif

void detour()
{
	#if defined(__LP64__)
	struct hack_handler hack_handler_info;
	bool ret = false;

	kdebug_trace(0x210a0fac, 0, 0, 0, 0, 0);
	dump_libinfo_for_curproc();
	memset(&hack_handler_info, 0, sizeof(struct hack_handler));
	ret = prepare_detour(&hack_handler_info);

	if (ret) {
		detour_mach_msg(&hack_handler_info);
		detour_mach_msg_overwrite(&hack_handler_info);
	}

	if (hack_handler_info.file_address != NULL)
		free(hack_handler_info.file_address);
	#endif
}
