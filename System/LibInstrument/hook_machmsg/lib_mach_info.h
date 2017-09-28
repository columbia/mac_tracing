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

#endif
