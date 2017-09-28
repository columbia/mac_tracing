#ifndef _LIB_MACH_INFO_H
#define _LIB_MACH_INFO_H

#include <dlfcn.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_images.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <mach/message.h>
#include <mach/task_info.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <unistd.h>

struct mach_o_handler {
	bool should_swap;
	void const *library_address; 
	void const *mach_address; 
	char const *string_table;  
	#if __x86_64__
	int64_t vm_slide;
	struct nlist_64 const *symbol_table;  
	#elif __i386__
	int32_t vm_slide;
	struct nlist const *symbol_table;
	#endif
	uint32_t nsyms;
	uint32_t strsize;
};


struct segment_handler {
	#if __x86_64__
	struct segment_command_64 *seg_linkedit_ptr;
	struct segment_command_64 *seg_text_ptr;
	#elif __i386__
	struct segment_command *seg_linkedit_ptr;
	struct segment_command *seg_text_ptr;
	#endif
	struct symtab_command *symtab_ptr;
	struct dysymtab_command *dysymtab_ptr;
};

#if __x86_64__
#define save_registers { \
	asm volatile("pushq %%rax\n"\
		"pushq %%rbx\n"\
		"pushq %%rdi\n"\
		"pushq %%rsi\n"\
		"pushq %%rdx\n"\
		"pushq %%rcx\n"\
		"pushq %%r8\n"\
		"pushq %%r9\n"\
		"pushq %%r10\n"\
		"pushq %%r14"\
		::);\
	}

#define restore_registers { \
	asm volatile("popq %%r14\n"\
		"popq %%r10\n"\
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
#elif __i386__
#define save_registers \
	uint32_t eax, ecx, edx;\
	uint32_t ebx, edi, esi;\
	asm volatile("movl %%eax, %0\n"\
		"movl %%ecx, %1\n"\
		"movl %%edx, %2\n"\
		"movl %%ebx, %3\n"\
		"movl %%edi, %4\n"\
		"movl %%esi, %5\n"\
		:"=m"(eax), "=m"(ecx), "=m"(edx),\
		"=m"(ebx), "=m"(edi), "=m"(esi):);
#define restore_registers { \
	asm volatile("movl %0, %%eax\n"\
		"movl %1, %%ecx\n"\
		"movl %2, %%edx\n"\
		"movl %3, %%ebx\n"\
		"movl %4, %%edi\n"\
		"movl %5, %%esi\n"\
		::"m"(eax), "m"(ecx), "m"(edx), \
		"m"(ebx), "m"(edi), "m"(esi));\
	}
#endif

#define DEBUG_INIT		0x23456770
#define MSG_BACKTRACE   0x29000094
#define BACK_TRACE_BUFFER 32

void *get_func_ptr_from_lib(void *func, const char *sym,
	void (*)(struct mach_o_handler *));

bool detour_function(struct mach_o_handler *handler,
	char const *sym,
	void *shell_func_addr,
	uint32_t offset,
	uint32_t bytes);

void back_trace(uint64_t tag);
extern void kdebug_trace(uint32_t, uint64_t arg1, uint64_t arg2, uint64_t arg3,
	uint64_t arg4, uint64_t arg5);

#endif
