#ifndef _MACH_INFO_H
#define _MACH_INFO_H

#include <sys/types.h>
#include <sys/mman.h>
#include <mach/mach_types.h>
//#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
//#include <mach-o/swap.h>
#include <mach-o/loader.h>
#include <string.h>
//#include <unistd.h>

#define VM_MIN_KERNEL_ADDRESS       ((vm_offset_t) 0xFFFFFF8000000000UL)
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS  (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)

typedef struct mach_header_64 * mach_header_64_t;

struct handler {
	void *text_base;
	void *mach_address;
	char *string_table;  
	struct nlist_64 const *symbol_table;  
	uint32_t nsyms;
	int64_t vm_slide;
    
	struct segment_command_64 *seg_linkedit_ptr;
	struct segment_command_64 *seg_text_ptr;
	struct symtab_command *symtab_ptr;
};

int prepare_detour(struct handler *handler_ptr);
void * get_local_sym_in_vm(struct handler *hlr, char const *sym_name);

#endif
