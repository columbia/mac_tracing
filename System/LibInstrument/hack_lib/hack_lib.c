#include "lib_mach_info.h"
#define DEBUG 0

static bool should_swap = true;
static inline bool fat_need_swap(uint32_t magic)
{
	if (magic == FAT_CIGAM)
		return true;
	return false;
}

static inline bool mach_need_swap(uint32_t magic)
{
	if (magic == MH_CIGAM_64)
		return true;
	return false;
}

static mach_header_64_t load_mach_64_from_fat(void * obj, bool should_swap)
{
	int header_size = sizeof(struct fat_header);
	int arch_size = sizeof(struct fat_arch);
	
	fat_header_t header = (fat_header_t)obj;
	struct fat_header dup_header = *header;
	if (should_swap)
		swap_fat_header(&dup_header, 0);
	
	int arch_offset = header_size;
	for (int i = 0; i < dup_header.nfat_arch; i++) {
		fat_arch_t arch = (fat_arch_t)(obj + arch_offset);
		struct fat_arch dup_arch = *arch;
		if (should_swap)
			swap_fat_arch(&dup_arch, 1, 0);
		int mach_header_offset = dup_arch.offset;
		arch_offset += arch_size;
		uint32_t magic = *((uint32_t *)(obj + mach_header_offset));
		if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
			return (mach_header_64_t)(obj + mach_header_offset);
		}
	}
	return NULL;
}

static void *get_mach(void *obj)
{
    if (obj == NULL) 
        return NULL;

	uint32_t magic = *((uint32_t*)(obj));
	mach_header_64_t img = NULL;
	
	if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
		should_swap = fat_need_swap(magic);
		img = load_mach_64_from_fat(obj, should_swap);
	} 
	else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
		should_swap = mach_need_swap(magic);
		img = (struct mach_header_64 *)obj;
	}
	
    if(img == NULL) {
        return NULL;
	}
	
	magic = *((uint32_t *)(img));
	should_swap = mach_need_swap(magic);

	if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
		return (void *)img;
	} else
		return NULL;
}

static bool get_dlinfo(void * func, struct hack_handler *hack_hlr)
{
	Dl_info dl_info;
	if (!dladdr(func, &dl_info))
		return false;

	hack_hlr->library_address = dl_info.dli_fbase;
	hack_hlr->mach_address = get_mach(dl_info.dli_fbase);

	if (hack_hlr->mach_address == NULL)
		return false;

	return true;
}

static bool load_segments(struct hack_handler *hack_hlr, struct hack_segments * segments_info)
{
	struct mach_header_64 dup_image = *((mach_header_64_t)(hack_hlr->mach_address));
    struct load_command *cmd = (struct load_command*)((mach_header_64_t)(hack_hlr->mach_address) + 1);
	struct load_command dup_cmd = *cmd;

	if (should_swap) {
		swap_mach_header_64(&dup_image, 0);
		swap_load_command(&dup_cmd, 0);
	}

    for (int index = 0; index < dup_image.ncmds; 
		index++, cmd = (struct load_command*)((void *)cmd + dup_cmd.cmdsize)) { 
		dup_cmd = *cmd;

		if (should_swap)
			swap_load_command(&dup_cmd, 0);

        switch(dup_cmd.cmd) {
            case LC_SEGMENT_64: {
                struct segment_command_64* segcmd = (struct segment_command_64*)cmd;
				struct segment_command_64 dup_segcmd = *segcmd;
				if (should_swap)
					swap_segment_command_64(&dup_segcmd, 0);
                if (!strcmp(dup_segcmd.segname, SEG_TEXT)) {
                    segments_info->seg_text_ptr = segcmd;
					hack_hlr->vm_slide = (uint64_t)(hack_hlr->library_address - dup_segcmd.vmaddr);
				}
                else if (!strcmp(dup_segcmd.segname, SEG_LINKEDIT))
                    segments_info->seg_linkedit_ptr = segcmd;
                break;
            }

            case LC_SYMTAB:
                segments_info->symtab_ptr = (struct symtab_command*)cmd;
                break;

			case LC_DYSYMTAB:
				segments_info->dysymtab_ptr = (struct dysymtab_command*)cmd;
				break;

			case LC_REEXPORT_DYLIB:
				break;

            default:
                break;
        }
    }

#if DEBUG
	printf("\n\nmach_handler_info:\n\t\t library mach image mapped in [vm] = %llx;\n\t\tloaded in [vm] = %llx;\n",
			hack_hlr->mach_address, hack_hlr->library_address);
	printf("\n\nsegment info[file]:\n\t\tsymtab = %llx;\n\t\tdysymtab = %llx;\n\t\tseg_linkedit = %llx;\n",
			segments_info->symtab_ptr, segments_info->dysymtab_ptr, segments_info->seg_linkedit_ptr);
#endif

	if (segments_info->symtab_ptr == NULL || segments_info->seg_linkedit_ptr == NULL)
		return false;

	return true;
}

static bool get_symbol_tables(struct hack_handler *hack_hlr)
{
	struct hack_segments segments_info = {0, 0, 0, 0};
	bool ret = load_segments(hack_hlr, &segments_info);
	if (ret == false)
		return false;

	struct symtab_command dup_symtab = *(segments_info.symtab_ptr);

	if (should_swap)
		swap_symtab_command(&dup_symtab, 0);

	struct segment_command_64 dup_linkedit = *(segments_info.seg_linkedit_ptr);
	if (should_swap)
		swap_segment_command_64(&dup_linkedit, 0);

	char * linkEditBase = (char *)(dup_linkedit.vmaddr - dup_linkedit.fileoff + hack_hlr->vm_slide);
	struct nlist_64 *symbols = (struct nlist_64 *)((unsigned long)(linkEditBase) + dup_symtab.symoff);
	char *strings = (char *)((unsigned long)(linkEditBase) + dup_symtab.stroff);

	hack_hlr->string_table = strings;
	hack_hlr->symbol_table = symbols;
	hack_hlr->nsyms = dup_symtab.nsyms;
	hack_hlr->strsize = dup_symtab.strsize;

#if DEBUG
	printf("\n\nsymble table info:\n");
	printf("\t\tsymbol_table [vm] = %p, ", hack_hlr->symbol_table);
	printf("\t\tnumbers of synmbols = %llx\n", hack_hlr->nsyms);
#endif
	return true;
}

static bool prepare_detour(void * func, struct hack_handler *hack_handler_ptr)
{
	//get file load address and vm address
	bool ret = get_dlinfo(func, hack_handler_ptr);
	if (ret == false)
		return false;
	//get symbol tables
	ret = get_symbol_tables(hack_handler_ptr);
	if (ret == false)
		return false;
	return true;
}

static void * get_local_sym_in_vm(struct hack_handler *hack_hlr, char const *sym_name)
{
	struct nlist_64 *symbase = (struct nlist_64 *)hack_hlr->symbol_table;
	const char* strings = hack_hlr->string_table;
	struct nlist_64 *sym = symbase;
	struct nlist_64 dup_sym;
	int64_t sym_offset = -1;

	for (int64_t index = 0; index < hack_hlr->nsyms; index++, sym++) {
		dup_sym = *sym;
		if (should_swap)
			swap_nlist_64(&dup_sym, 1, 0);
		if (dup_sym.n_un.n_strx != 0 && !strcmp(sym_name, strings + dup_sym.n_un.n_strx + 1)) {
			sym_offset = index;
			break;
		}
	}
	if (sym_offset ==  -1)
		return NULL;
	
	uint64_t ret = hack_hlr->vm_slide + dup_sym.n_value;
	
	#if DEBUG
	//dump the binary code from the function for check
	uint8_t * dump_code = (uint8_t *)(ret);
	printf("Dump funciton for check:");
	for (int i = 0; i < 64; i++, dump_code++) {
		if (i % 16 == 0)
			printf("\n \t\t");
		uint8_t opcode = *dump_code;
		printf("%x  ", opcode);
	}
	printf("\n\n");
	#endif	

	return (void *)(ret);
}

/*
 * change opcode in text section
 * first 5+ opcode are used;
 * depends on original function instructions layout
 */
static char invoke_hook[] =
		"\xe8\xcc\x01\x00\x00" /*call shell code*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
;

static int init_code(uint64_t victim_func, uint64_t shell_func)
{
	uint64_t ret_offset = shell_func - (victim_func + 5);
	*(uint32_t*)&invoke_hook[1] = (uint32_t)ret_offset;

	#if DEBUG
	printf("calculated offset: \n");
	printf("\t\tret_offset = %llx\n", ret_offset);
	#endif

	return 0;
}

bool detour_function(struct hack_handler * hack_handler_ptr,
					 char const * sym,
					 void * shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes)
{
	void *sym_vm_addr = get_local_sym_in_vm(hack_handler_ptr, sym);
	if (sym_vm_addr == NULL)
		return false;

	//check if the fuction lay out accrosses page boarder
	int range = PAGE_SIZE;
	if (((uint32_t)(sym_vm_addr + offset + bytes) & PAGE_MASK) < bytes)
		range = range * 2;

	int ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)), range, PROT_READ|PROT_WRITE|PROT_EXEC);
	if (ret < 0)
		return false;

	init_code((uint64_t)sym_vm_addr + offset, (uint64_t)shell_func_addr);
	memcpy((char *)((uint64_t)sym_vm_addr + offset), invoke_hook, bytes);

	ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)), range, PROT_READ|PROT_EXEC);
	if (ret < 0)
		exit(EXIT_FAILURE);

	#if DEBUG
	printf("successfully copied %d bytes code to %p\n", bytes, (uint64_t)(sym_vm_addr + offset));
	sym_vm_addr = get_local_sym_in_vm(hack_handler_ptr, sym);
	if (sym_vm_addr == NULL)
		return false;
	#endif
	return true;
}

void * get_func_ptr_from_lib(void * func, const char * sym)
{
	struct hack_handler hack_handler_info;

	memset(&hack_handler_info, 0, sizeof(struct hack_handler));
	if (!prepare_detour(func, &hack_handler_info))
		exit(EXIT_FAILURE);

	void * sym_vm_addr = get_local_sym_in_vm(&hack_handler_info, sym);
	if(sym_vm_addr == NULL) {
		printf("Not found\n");
		exit(EXIT_FAILURE);
	}
	#if DEBUG
	else {
		printf("%s : %llx\n", sym, sym_vm_addr);
	}
	#endif

	#if defined(__LP64__)
	/* defined in patch_***.c */
	detour(&hack_handler_info);
	#endif
	return sym_vm_addr;
}

void back_trace(uint64_t func)
{
	#if defined(__LP64__)
 	void *callstack[BACK_TRACE_BUFFER] = {(void *)0};
	uint32_t frame_index = 0;
	vm_offset_t stackptr, stackptr_prev, raddr;

	__asm__ volatile("movq %%rbp, %0" : "=m"(stackptr));
	/*
	while (frame_index < 2) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *)stackptr_prev);
		frame_index++;
	}
	*/

	for (frame_index = 0; frame_index < BACK_TRACE_BUFFER; frame_index++) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *) stackptr_prev);
		if (stackptr < stackptr_prev)
			break;
		raddr = *((vm_offset_t *)(stackptr + 8));
		if (raddr < 4096)
			break;
		callstack[frame_index] = (void*) raddr;
	}

	kdebug_trace(MSG_BACKTRACE, func, frame_index, callstack[0], callstack[1], 0);
 	for (int i = 2; i < frame_index && i + 2 < BACK_TRACE_BUFFER; i += 3) {
		if (callstack[i] == (void*)0)
			break;
		kdebug_trace(MSG_BACKTRACE, func, callstack[i], callstack[i + 1], callstack[i + 2], 0);
	}
	#endif
}

/* lib test
int main(int argc, char * argv[])
{
	char const *sym = argv[1];
	struct hack_handler hack_handler_info;
	memset(&hack_handler_info, 0, sizeof(struct hack_handler));
	prepare_detour(pthread_getname_np, &hack_handler_info);
	void * sym_vm_addr = get_local_sym_in_vm(&hack_handler_info, sym);
	if(sym_vm_addr == NULL) {
		printf("Not found\n");
		exit(EXIT_FAILURE);
	}
}
*/
