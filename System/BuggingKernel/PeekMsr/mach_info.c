#include "mach_info.h"

static int should_swap = 1;

static inline int mach_need_swap(uint32_t magic)
{
	if (magic == MH_CIGAM_64)
		return 1;
	return 0;
}


static void *get_mach(uint8_t *text_base)
{
    if (text_base == NULL)
        return NULL;
    
	uint32_t magic = *((uint32_t*)(text_base));
	mach_header_64_t img = NULL;

    while ((uintptr_t)text_base >= VM_MIN_KERNEL_ADDRESS) {
        magic = *((uint32_t*)(text_base));
        if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
            should_swap = mach_need_swap(magic);
            img = (struct mach_header_64 *)text_base;
            break;
        }
        text_base--;
    }

	return (void *)img;
}


static int load_segments(struct handler *hlr)
{
	struct mach_header_64 dup_image = *((mach_header_64_t)(hlr->mach_address));
    struct load_command *cmd = (struct load_command*)((mach_header_64_t)(hlr->mach_address) + 1);
	struct load_command dup_cmd = *cmd;

	/*
	if (should_swap) {
		swap_mach_header_64(&dup_image, 0);
		swap_load_command(&dup_cmd, 0);
	}
	*/

    for (int index = 0; index < dup_image.ncmds; 
		index++, cmd = (struct load_command*)((void *)cmd + dup_cmd.cmdsize)) { 
		dup_cmd = *cmd;

	/*
		if (should_swap)
			swap_load_command(&dup_cmd, 0);
	*/

        switch(dup_cmd.cmd) {
            case LC_SEGMENT_64: {
                struct segment_command_64* segcmd = (struct segment_command_64*)cmd;
				struct segment_command_64 dup_segcmd = *segcmd;
		/*
				if (should_swap)
					swap_segment_command_64(&dup_segcmd, 0);
		*/
                if (!strcmp(dup_segcmd.segname, SEG_TEXT)) {
                    hlr->seg_text_ptr = segcmd;
					hlr->vm_slide =  hlr->mach_address - dup_segcmd.vmaddr;
					printf("\tget_segtext 0x%08x%08x vmslide = 0x%08x%08x\n", 
						(uint32_t)((uint64_t)segcmd >> 32), (uint32_t)(segcmd),
						(uint32_t)((uint64_t)hlr->vm_slide >>32), (uint32_t)(hlr->vm_slide));
				}
                else if (!strcmp(dup_segcmd.segname, SEG_LINKEDIT)) {
                    hlr->seg_linkedit_ptr = segcmd;
					printf("\tget_linkedit 0x%08x%08x\n",
						(uint32_t)((uint64_t)segcmd >> 32), (uint32_t)(segcmd));
				}
                break;
            }

            case LC_SYMTAB: {
                hlr->symtab_ptr = (struct symtab_command*)cmd;
				printf("\tget symtab_ptr 0x%08x%08x\n",
						(uint32_t)((uint64_t)cmd >> 32), (uint32_t)(cmd));
                break;
			}

			case LC_REEXPORT_DYLIB:
				break;

            default:
                break;
        }
    }

	if (hlr->symtab_ptr == NULL)
		return 0;
	return 1;
}

static int get_symbol_table(struct handler * hlr)
{
	int ret = load_segments(hlr);
	if (ret == 0)
		return 0;

	struct symtab_command dup_symtab = *(hlr->symtab_ptr);

	/*
	if (should_swap)
		swap_symtab_command(&dup_symtab, 0);
	*/
	char * linkEditBase = (char *)(hlr->seg_linkedit_ptr->vmaddr) - hlr->seg_linkedit_ptr->fileoff + hlr->vm_slide;
	struct nlist_64 *symbase = (struct nlist_64 *)((unsigned long)(linkEditBase) + dup_symtab.symoff);
	char *strings = (char *)((unsigned long)(linkEditBase) + dup_symtab.stroff);
	
	hlr->string_table = strings;
	hlr->symbol_table = symbase;
	hlr->nsyms = dup_symtab.nsyms;
	return 1;
}

int prepare_detour(struct handler *handler_ptr)
{
	//get file load address and vm address
	handler_ptr->mach_address = get_mach(handler_ptr->text_base);
	if (!handler_ptr->mach_address) {
		printf("fail to get mach_addr\n");
		return 0;
	}

	uint32_t hi = (uint32_t)((uint64_t)handler_ptr->mach_address >> 32);
	uint32_t lo = (uint32_t)(handler_ptr->mach_address);
	printf("get mach_addr 0x%08x%08x\n", hi, lo);

	//get symbol tables
	int ret = get_symbol_table(handler_ptr);
	return ret;
	
}

void * get_local_sym_in_vm(struct handler *hlr, char const *sym_name)
{
	struct nlist_64 *symbase = (struct nlist_64 *)hlr->symbol_table;
	const char* strings = hlr->string_table;
	struct nlist_64 *sym = symbase;
	struct nlist_64 dup_sym;
	int64_t sym_offset = -1;

	/*
	for (int i = 0 ; i < 4096; i++) {
		printf("%c", *((char *)strings+i));
		if (i % 65 == 64)
			printf("\n");
	}
	*/

	for (int64_t index = 0; index < hlr->nsyms; index++, sym++) {
		dup_sym = *sym;
		/*
		if (should_swap)
			swap_nlist_64(&dup_sym, 1, 0);
		*/
		if (dup_sym.n_un.n_strx != 0 && !strcmp(sym_name, strings + dup_sym.n_un.n_strx + 1)) {
			sym_offset = index;
			break;
		}
	}
	if (sym_offset ==  -1)
		return NULL;
	
	uint64_t ret = hlr->vm_slide + dup_sym.n_value;
	
	#if 1 //DEBUG
	//dump the binary code from the function for check
	uint8_t * dump_code = (uint8_t *)(ret);
	printf("Dump funciton %s for check:", sym_name);
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
** change opcode in text section
** first 5+ opcode are used;
** depends on original function instructions layout
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

/*
int detour_function(struct handler * handler_ptr,
					 char const * sym,
					 void * shell_func_addr,
					 uint32_t offset,
					 uint32_t bytes)
{
	void *sym_vm_addr = get_local_sym_in_vm(handler_ptr, sym);
	if (sym_vm_addr == NULL)
		return 0;

	//check if the fuction lay out accrosses page boarder
	int range = PAGE_SIZE;
	if (((uint32_t)(sym_vm_addr + offset + bytes) & PAGE_MASK) < bytes)
		range = range * 2;

	int ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)), range, PROT_READ|PROT_WRITE|PROT_EXEC);
	if (ret < 0)
		return 0;

	init_code((uint64_t)sym_vm_addr + offset, (uint64_t)shell_func_addr);
	memcpy((char *)((uint64_t)sym_vm_addr + offset), invoke_hook, bytes);

	ret = mprotect((caddr_t)((uint64_t)(sym_vm_addr + offset) & (~PAGE_MASK)), range, PROT_READ|PROT_EXEC);
	if (ret < 0)
		return 0;

	#if DEBUG
	printf("successfully copied %d bytes code to %p\n", bytes, (uint64_t)(sym_vm_addr + offset));
	sym_vm_addr = get_local_sym_in_vm(handler_ptr, sym);
	if (sym_vm_addr == NULL)
		return 0;
	#endif
	return 1;
}
*/
