#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _MYSYMBOLICATOR_LIB_MACH_INFO_H
#define _MYSYMBOLICATOR_LIB_MACH_INFO_H
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
#include <mach/mach_traps.h>
#include <mach/mach_vm.h>
#include <libproc.h>
#include <unistd.h>

//namespace mysymbolicator
//{
	typedef struct mach_header_64 * mach_header_64_t;
	typedef struct fat_header * fat_header_t;
	typedef struct fat_arch * fat_arch_t;

	struct vm_sym {
		uint64_t vm_offset;
		uint64_t str_index;
	};

	struct hack_handler {
		void const *mach_address; 
		char const *string_table;  
		struct nlist_64 const *symbol_table;  
		uint32_t nsyms;
		uint32_t strsize;
		
		char * strings;
		struct vm_sym* symbol_arrays; 

		void const *library_address; 
		int64_t vm_slide;
		

		/*import table
		  uint32_t const *indirect_table;
		  uint32_t nindirectsyms; 
		  uint32_t nundefsym;
		  uint32_t iundefsym;

		  uint32_t iindirectsyms; 
		  uint64_t import_table_addr;  
		  uint64_t import_table_size;
		 */

		/*text section
		  uint64_t text_sect_addr;
		  uint64_t text_sect_offset;
		 */
	};

	struct hack_segments {
		struct segment_command_64 *seg_linkedit_ptr;
		struct segment_command_64 *seg_text_ptr;
		struct symtab_command *symtab_ptr;
		struct dysymtab_command *dysymtab_ptr;
	};
	
	bool get_syms_for_libpath(pid_t pid, const char *path, struct hack_handler * hack_hlr_ptr);
	//uint64_t get_lib_load_addr(mach_port_t target_task, const char * path);
	//int64_t get_local_syms_in_vm(mach_port_t target_task, struct hack_handler *hack_hlr);
	//const char * get_sym_for_addr(pid_t pid, uint64_t vmoffset, const char * path);
//}
#endif
#ifdef __cplusplus
}
#endif
