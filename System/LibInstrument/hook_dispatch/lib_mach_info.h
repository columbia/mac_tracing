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
//#include <Block.h>
#include <dispatch/dispatch.h>

typedef struct mach_header_64 * mach_header_64_t;
typedef struct fat_header * fat_header_t;
typedef struct fat_arch * fat_arch_t;

struct hack_handler {
	void *file_address; 
	void const *mach_address; 
	char const *string_table;  
	struct nlist_64 const *symbol_table;  
	uint32_t nsyms;
	
	void const *library_address; 
	int64_t vm_slide;
	
	/*import table*/
	uint32_t const *indirect_table;
	uint32_t nindirectsyms; 
	uint32_t nundefsym;
	uint32_t iundefsym;

	uint32_t iindirectsyms; 
	uint64_t import_table_addr;  
	uint64_t import_table_size;
	
	/*text section*/
	uint64_t text_sect_addr;
	uint64_t text_sect_offset;

};

struct hack_segments {
	struct segment_command_64 *seg_linkedit_ptr;
	struct segment_command_64 *seg_text_ptr;
	struct symtab_command *symtab_ptr;
	struct dysymtab_command *dysymtab_ptr;
};

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

extern bool prepare_detour(struct hack_handler *hack_handler_ptr);
extern bool detour_function(struct hack_handler * hack_handler_ptr,
					 char const * sym,
					 void * shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes);

extern void detour_enqueue(struct hack_handler *hack_handler_ptr);
extern void detour_dequeue(struct hack_handler *hack_handler_ptr);
extern void detour_blockinvoke(struct hack_handler *hack_handler_ptr);

#define DISPATCH_ENQUEUE 0x210a0008
#define DISPATCH_DEQUEUE 0x210a000c
#define DISPATCH_EXECUTE 0x210a0010
#define CALLOUT_BEGIN 0
#define CALLOUT_END 1

#endif
