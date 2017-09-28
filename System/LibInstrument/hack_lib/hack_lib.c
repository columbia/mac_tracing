/* detour functions in libsystem_kernel.dylib
 * arch x86_64 and i386
 **************************/

#include "lib_mach_info.h"
#define DEBUG 0

static inline bool fat_need_swap(uint32_t magic)
{
	if (magic == FAT_CIGAM)
		return true;
	return false;
}

static inline bool mach_need_swap(uint32_t magic)
{
	if (magic == MH_CIGAM_64 || magic == MH_CIGAM)
		return true;
	return false;
}

static void *load_mach_from_fat(void *obj, bool should_swap) 
{
	int header_size = sizeof(struct fat_header);
	int arch_size = sizeof(struct fat_arch);
	int arch_offset = header_size;
	struct fat_header dup_header = *(struct fat_header *)obj;

	if (should_swap)
		swap_fat_header(&dup_header, 0);
	
	for (int i = 0; i < dup_header.nfat_arch;
		i++, arch_offset += arch_size) {
		struct fat_arch dup_arch = *(struct fat_arch *)(obj + arch_offset);
		if (should_swap)
			swap_fat_arch(&dup_arch, 1, 0);

		uint32_t magic = *((uint32_t *)(obj + dup_arch.offset));
#if __x86_64__
		if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64)
			return obj + dup_arch.offset;
#elif __i386__
		if ( magic == MH_MAGIC || magic == MH_CIGAM)
			return obj + dup_arch.offset;
#endif
	}
	return NULL;
}

static void *get_mach(void *obj, bool *should_swap)
{
    if (!obj) 
        return NULL;

	uint32_t magic = *((uint32_t*)(obj));
	void *img = obj;
	
	if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
		img = load_mach_from_fat(obj, fat_need_swap(magic));
		if (!img)
			return NULL;
		magic = *((uint32_t *)(img));
	}
	
	*should_swap = mach_need_swap(magic);

#if __x86_64__
	if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64)
#elif __i386__
	if (magic == MH_MAGIC || magic == MH_CIGAM)
#else
	if (false)
#endif
		return img;

	return NULL;
}

static bool get_dlinfo(void *func, struct mach_o_handler *handler)
{
	Dl_info dl_info;
	if (!dladdr(func, &dl_info))
		return false;

	handler->library_address = dl_info.dli_fbase;
	handler->mach_address = get_mach(dl_info.dli_fbase,
							&(handler->should_swap));

	if (handler->mach_address == NULL)
		return false;

	return true;
}

#if __x86_64__
static bool load_segments_64(struct mach_o_handler *handler, 
	struct segment_handler *segments_info)
{
	struct mach_header_64 dup_image = 
				*((struct mach_header_64 *)(handler->mach_address));
	struct load_command * cmd = (struct load_command *)(
				(struct mach_header_64 *)(handler->mach_address) + 1);
	struct load_command dup_cmd = *cmd;

	if (handler->should_swap) {
		swap_mach_header_64(&dup_image, 0);
		swap_load_command(&dup_cmd, 0);
	}

    for (int index = 0; index < dup_image.ncmds; 
		index++, cmd = (struct load_command *)((void *)cmd + dup_cmd.cmdsize)) { 
		dup_cmd = *cmd;

		if (handler->should_swap)
			swap_load_command(&dup_cmd, 0);

        switch(dup_cmd.cmd) {
            case LC_SEGMENT_64: {
				struct segment_command_64 dup_segcmd
						= *(struct segment_command_64 *)cmd;
				if (handler->should_swap)
					swap_segment_command_64(&dup_segcmd, 0);

                if (!strcmp(dup_segcmd.segname, SEG_TEXT)) {
                    segments_info->seg_text_ptr
						= (struct segment_command_64 *)cmd;
					handler->vm_slide = (uint64_t)(handler->library_address
									- dup_segcmd.vmaddr);
				} else if (!strcmp(dup_segcmd.segname, SEG_LINKEDIT))
                    segments_info->seg_linkedit_ptr
						= (struct segment_command_64 *)cmd;
                break;
            }

            case LC_SYMTAB:
                segments_info->symtab_ptr = (struct symtab_command *)cmd;
                break;

			case LC_DYSYMTAB:
				segments_info->dysymtab_ptr = (struct dysymtab_command *)cmd;
				break;

			case LC_REEXPORT_DYLIB:
				break;

            default:
                break;
        }
    }

#if DEBUG
	printf("mach-o info:\
			\n\t\tlibrary loaded[vm] = %p;\
			\n\t\tmach-o mapped[vm] = %p;\
			\n\t\tvm_slide = %p;\
			\n\t\tsymtab = %p;\
			\n\t\tdysymtab = %p;\
			\n\t\tseg_linkedit = %p;\n",
			handler->library_address,
			handler->mach_address,
			handler->vm_slide,
			segments_info->symtab_ptr,
			segments_info->dysymtab_ptr,
			segments_info->seg_linkedit_ptr);
#endif

	if (segments_info->symtab_ptr == NULL
		|| segments_info->seg_linkedit_ptr == NULL)
		return false;

	return true;
}
#elif __i386__
static bool load_segments_32(struct mach_o_handler *handler,
	struct segment_handler *segments_info)
{
	struct mach_header dup_image =
				*((struct mach_header *)(handler->mach_address));
	struct load_command * cmd = (struct load_command *)(
				(struct mach_header *)(handler->mach_address) + 1);
	struct load_command dup_cmd = *cmd;

	if (handler->should_swap) {
		swap_mach_header(&dup_image, 0);
		swap_load_command(&dup_cmd, 0);
	}

    for (int index = 0; index < dup_image.ncmds; 
		index++, cmd = (struct load_command *)((void *)cmd + dup_cmd.cmdsize)) { 
		dup_cmd = *cmd;

		if (handler->should_swap)
			swap_load_command(&dup_cmd, 0);

        switch(dup_cmd.cmd) {
            case LC_SEGMENT: {
				struct segment_command dup_segcmd
					= *(struct segment_command *)cmd;
				if (handler->should_swap)
					swap_segment_command(&dup_segcmd, 0);

                if (!strcmp(dup_segcmd.segname, SEG_TEXT)) {
                    segments_info->seg_text_ptr = (struct segment_command *)cmd;
					handler->vm_slide = (int32_t)(handler->library_address
									- dup_segcmd.vmaddr);
				} else if (!strcmp(dup_segcmd.segname, SEG_LINKEDIT))
                    segments_info->seg_linkedit_ptr
						= (struct segment_command *)cmd;
                break;
            }

            case LC_SYMTAB:
                segments_info->symtab_ptr = (struct symtab_command *)cmd;
                break;

			case LC_DYSYMTAB:
				segments_info->dysymtab_ptr = (struct dysymtab_command *)cmd;
				break;

			case LC_REEXPORT_DYLIB:
				break;

            default:
                break;
        }
    }

#if DEBUG
	printf("mach-o info:\
			\n\t\tlibrary loaded[vm] = %p;\
			\n\t\tmach-o mapped[vm] = %p;\
			\n\t\tvm_slide = %p;\
			\n\t\tsymtab = %p;\
			\n\t\tdysymtab = %p;\
			\n\t\tseg_linkedit = %p;\n",
			handler->library_address,
			handler->mach_address,
			handler->vm_slide,
			segments_info->symtab_ptr,
			segments_info->dysymtab_ptr,
			segments_info->seg_linkedit_ptr);
#endif

	if (segments_info->symtab_ptr == NULL
		|| segments_info->seg_linkedit_ptr == NULL)
		return false;
	return true;
}
#endif

static bool get_symbol_tables(struct mach_o_handler *handler)
{
	struct segment_handler segments_info = {0, 0, 0, 0};

#if __x86_64__
	bool ret = load_segments_64(handler, &segments_info);
#elif __i386__
	bool ret = load_segments_32(handler, &segments_info);
#else
	return false;
#endif

	if (ret == false)
		return false;

	struct symtab_command dup_symtab = *(segments_info.symtab_ptr);
	if (handler->should_swap)
		swap_symtab_command(&dup_symtab, 0);

#if __x86_64__
	struct segment_command_64 dup_linkedit = *(segments_info.seg_linkedit_ptr);
	if (handler->should_swap)
		swap_segment_command_64(&dup_linkedit, 0);
#elif __i386__
	struct segment_command dup_linkedit = *(segments_info.seg_linkedit_ptr);
	if (handler->should_swap)
		swap_segment_command(&dup_linkedit, 0);
#endif

	char *linkEditBase = (char *)(dup_linkedit.vmaddr - dup_linkedit.fileoff
					+ handler->vm_slide);

#if __x86_64__
	struct nlist_64 *symbols = (struct nlist_64 *)((uint64_t)(linkEditBase)
						+ dup_symtab.symoff);
	char *strings = (char *)((uint64_t)(linkEditBase) + dup_symtab.stroff);
#elif __i386__
	struct nlist *symbols = (struct nlist *)((uint32_t)(linkEditBase)
						+ dup_symtab.symoff);
	char *strings = (char *)((uint32_t)(linkEditBase) + dup_symtab.stroff);
#endif

	handler->string_table = strings;
	handler->symbol_table = symbols;
	handler->nsyms = dup_symtab.nsyms;
	handler->strsize = dup_symtab.strsize;

#if DEBUG
	printf("symble table info: symbol_table [%x] = %p\n",
			handler->nsyms,
			handler->symbol_table);
#endif
	return true;
}

static bool prepare_detour(void *func, struct mach_o_handler *handler)
{
	//get file load address and vm address
	if (!get_dlinfo(func, handler))
		return false;

	//get symbol tables
	if (!get_symbol_tables(handler))
		return false;

	return true;
}

static void *get_local_sym_in_vm(struct mach_o_handler *handler,
			char const *sym_name)
{
	const char *strings = handler->string_table;
#if __x86_64__
	struct nlist_64 *sym = (struct nlist_64 *)handler->symbol_table;
	struct nlist_64 dup_sym;
	int64_t sym_offset = -1;
#elif __i386__
	struct nlist *sym = (struct nlist *)handler->symbol_table;
	struct nlist dup_sym;
	int32_t sym_offset = -1;
#endif

	for (int index = 0; index < handler->nsyms; index++, sym++) {
		dup_sym = *sym;
		if (handler->should_swap) {
#if __x86_64__
			swap_nlist_64(&dup_sym, 1, 0);
#elif __i386__
			swap_nlist(&dup_sym, 1, 0);
#endif
		}
		if (dup_sym.n_un.n_strx != 0 &&
			!strcmp(sym_name, strings + dup_sym.n_un.n_strx + 1)) {
			sym_offset = index;
			break;
		}
	}

	if (sym_offset ==  -1)
		return NULL;
	
	uint64_t ret = handler->vm_slide + dup_sym.n_value;
	
#if DEBUG
	//dump the binary code from the function for check
	uint8_t *dump_code = (uint8_t *)(ret);
	printf("Dump funciton %s for check:", sym_name);
	for (int i = 0; i < 32; i++, dump_code++) {
		if (i % 16 == 0)
			printf("\n\t\t");
		uint8_t opcode = *dump_code;
		printf("%x  ", opcode);
	}
	printf("\n\n");
#endif	

	return (void *)(ret);
}

static int init_code(uint64_t victim_func, uint64_t shell_func,
		char *invoke_hook)
{
	uint64_t ret_offset = shell_func - (victim_func + 5);

#if DEBUG
	printf("calculated offset:\n\t\tret_offset = %p\n", ret_offset);
#endif

	if (!(ret_offset < 0xffffffff
		|| (victim_func + 5) - shell_func < 0xffffffff ))
		return -EINVAL;

	*(uint32_t*)&invoke_hook[1] = (uint32_t)ret_offset;
	return 0;
}

bool detour_function(struct mach_o_handler *handler,
					 char const *sym,
					 void *shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes)
{
	void *sym_vm_addr = get_local_sym_in_vm(handler, sym);
	int range = PAGE_SIZE;
	int ret;

	if (sym_vm_addr == NULL)
		return false;
	/*
	 * change opcode in text section
	 * first 5+ opcode are used;
	 * depends on original function instructions layout
	 */
	char invoke_hook[] =
		"\xe8\x00\x00\x00\x00" /*call shell code*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		;

	if (init_code((uint64_t)sym_vm_addr + offset, (uint64_t)shell_func_addr,
		invoke_hook)) {
		perror("invalid jmp opcode\n");
		return false;
	}

	//check if the fuction lay out accrosses page boarder
	if (((uint32_t)(sym_vm_addr + offset + bytes) & PAGE_MASK) < bytes)
		range = PAGE_SIZE * 2;

	ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)),
		range, PROT_READ|PROT_WRITE|PROT_EXEC);
	if (ret < 0) {
		perror("protection failed 1\n");
		return false;
	}

	memcpy((char *)((uint64_t)sym_vm_addr + offset), invoke_hook, bytes);

	ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)),
		range, PROT_READ|PROT_EXEC);
	if (ret < 0) {
		perror("protection failed 2\n");
		exit(EXIT_FAILURE);
	}

#if DEBUG
	printf("successfully copied %d bytes code to %p\n",
		bytes,
		sym_vm_addr + offset);
	sym_vm_addr = get_local_sym_in_vm(handler, sym);
	if (sym_vm_addr == NULL)
		return false;
#endif

	return true;
}

void *get_func_ptr_from_lib(void *func, const char *sym,
		void (*detour_callback)(struct mach_o_handler *))
{
	struct mach_o_handler handler;

	memset(&handler, 0, sizeof(struct mach_o_handler));
	if (!prepare_detour(func, &handler))
		return NULL;

	void *sym_vm_addr = get_local_sym_in_vm(&handler, sym);
	if(sym_vm_addr == NULL) {
		perror("sym Not found\n");
	} else if(detour_callback) {
#if DEBUG
		printf("%s : %p\n", sym, sym_vm_addr);
#endif
		detour_callback(&handler);
	}

	return sym_vm_addr;
}

void back_trace(uint64_t func_tag)
{
 	uint64_t callstack[BACK_TRACE_BUFFER] = {0};
	uint64_t frame_index = 0;
	vm_offset_t stackptr, stackptr_prev, raddr;

#if __x86_64__
	__asm__ volatile("movq %%rbp, %0" : "=m"(stackptr):);
#elif __i386__
	__asm__ volatile("movl %%ebp, %0" : "=m"(stackptr):);
#endif
	
//	while (frame_index < 2) {
//		stackptr_prev = stackptr;
//		stackptr = *((vm_offset_t *)stackptr_prev);
//		frame_index++;
//	}
	for (frame_index = 0; frame_index < BACK_TRACE_BUFFER; frame_index++) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *) stackptr_prev);
		if (stackptr < stackptr_prev)
			break;
#if __x86_64__
		raddr = *((vm_offset_t *)(stackptr + 8));
#elif __i386__
		raddr = *((vm_offset_t *)(stackptr + 4));
#endif
		if (raddr < 4096)
			break;
		callstack[frame_index] = (uint64_t) raddr;
	}

	kdebug_trace(MSG_BACKTRACE, func_tag,
		frame_index,
		callstack[0],
		callstack[1],
		0);

 	for (int i = 2; i < frame_index && i + 2 < BACK_TRACE_BUFFER; i += 3) {
		if (callstack[i] == 0)
			break;
		if (i + 1 == frame_index) {
			callstack[i + 1] = 0;
			callstack[i + 2] = 0;
		} else if (i + 2 == frame_index) {
			callstack[i + 2] = 0;
		}
		
		kdebug_trace(MSG_BACKTRACE, func_tag,
			callstack[i],
			callstack[i + 1],
			callstack[i + 2],
			0);
	}
}

/* lib test
int main(int argc, char *argv[])
{
	char const *sym = argv[1];
	struct mach_o_handler handler_info;
	memset(&handler_info, 0, sizeof(struct mach_o_handler));
	if (prepare_detour(mach_msg, &handler_info)) {
		void *sym_vm_addr = get_local_sym_in_vm(&handler_info, sym);
		if(sym_vm_addr == NULL) {
			printf("Not found\n");
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}
*/
