#include "lib_mach_info.h"

#if defined(__LP64__)

#define MSG_PATHINFO 	0x29000090

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
	back_trace(msg_addr);
	restore_registers;
	//simulation
	asm volatile("movl %%r9d, %%ebx\n"
		"movl %%r8d, %%r15d"
		::);
}

/*_mach_msg_overwrite
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
	back_trace(msg_addr);
	restore_registers;
	//simulation
	asm volatile("movl %%r9d, %%ebx\n"
		"movl %%r8d, %%r15d"
		::);
}

void detour(struct hack_handler *hack_handler_ptr)
{
	kdebug_trace(DEBUG_INIT, 0, 0, 0, 0, 0);
	dump_libinfo_for_curproc();

	detour_function(hack_handler_ptr, "mach_msg", shell_mach_msg, 17, 6);
	detour_function(hack_handler_ptr, "mach_msg_overwrite", shell_mach_msg_overwrite, 17, 6);
}

#endif
