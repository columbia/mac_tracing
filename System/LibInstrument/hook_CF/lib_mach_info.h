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
#include <mach/message.h>
#include <mach/task_info.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <unistd.h>
#include <CoreFoundation/CFRunLoop.h>

typedef struct mach_header_64 * mach_header_64_t;
typedef struct fat_header * fat_header_t;
typedef struct fat_arch * fat_arch_t;

struct hack_handler {
	void const *library_address; 
	void const *mach_address; 
	char const *string_table;  
	struct nlist_64 const *symbol_table;  
	uint32_t nsyms;
	uint32_t strsize;
	int64_t vm_slide;
};


struct hack_segments {
	struct segment_command_64 *seg_linkedit_ptr;
	struct segment_command_64 *seg_text_ptr;
	struct symtab_command *symtab_ptr;
	struct dysymtab_command *dysymtab_ptr;
};

#if defined(__LP64__)
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

#endif

extern void * get_func_ptr_from_lib(void * func, const char * sym);
extern void detour();
extern bool detour_function(struct hack_handler * hack_handler_ptr,
					 char const * sym,
					 void * shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes);
extern bool prepare_detour(void * func, struct hack_handler *hack_handler_ptr);
//extern void detour_corefoundation(struct hack_handler * hack_handler_ptr);

#define DEBUG_INIT				0x23456770
#define DEBUG_CreateObserver	0x23456780
#define DEBUG_AddObserver		0x23456784
#define DEBUG_RemoveObserver	0x23456788
#define DEBUG_CallObserver		0x2345678c


#endif
