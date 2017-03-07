#include "lib_mach_info.h"
#define DEBUG 0

typedef struct mach_header_64 * mach_header_64_t;
typedef struct fat_header * fat_header_t;
typedef struct fat_arch * fat_arch_t;

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

	const char * filepath = dl_info.dli_fname;
	FILE *fp_bin;
	if (!(fp_bin = fopen(filepath, "rb")))
		return false;

	fseek(fp_bin, 0, SEEK_END);
	long fsize = ftell(fp_bin);
	fseek(fp_bin, 0, SEEK_SET);
	
	void *obj = malloc(fsize);
	if (obj == NULL || fread(obj, 1, fsize, fp_bin) != fsize) {
		fclose(fp_bin);
		return false;
	}
	fclose(fp_bin);

	hack_hlr->file_address = obj;
	hack_hlr->mach_address = get_mach(obj);
	hack_hlr->library_address = dl_info.dli_fbase;

	if (hack_hlr->mach_address == NULL) {
		hack_hlr->file_address = NULL;
		free(obj);
		return false;
	}
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
                if (!strcmp(dup_segcmd.segname, SEG_TEXT))
                    segments_info->seg_text_ptr = segcmd;
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
	printf("\n\nmach_handler_info:\n\t\t library mapped in [file] = %llx;\n\t\tloaded in [vm] = %llx;\n",
			hack_hlr->mach_address, hack_hlr->library_address);
	printf("\n\nsegment info[file]:\n\t\tsymtab = %llx;\n\t\tdysymtab = %llx;\n\t\tseg_linkedit = %llx;\n",
			segments_info->symtab_ptr, segments_info->dysymtab_ptr, segments_info->seg_linkedit_ptr);
#endif

	if (segments_info->symtab_ptr == NULL)
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

	struct nlist_64 *symbase = (struct nlist_64 *)((unsigned long)(hack_hlr->mach_address)
								+ dup_symtab.symoff);
	char *strings = (char *)((unsigned long)(hack_hlr->mach_address) 
								+ dup_symtab.stroff);
	
	hack_hlr->string_table = strings;
	hack_hlr->symbol_table = symbase;
	hack_hlr->nsyms = dup_symtab.nsyms;
	
	/* load undefsym information
	if (segments_info.dysymtab_ptr == NULL)
		return false;

	struct dysymtab_command dup_dysymtab = *(segments_info.dysymtab_ptr);
	if (should_swap)
		swap_dysymtab_command(&dup_dysymtab, 0);

	hack_hlr->nundefsym = dup_dysymtab.nundefsym;
	hack_hlr->iundefsym = dup_dysymtab.iundefsym;

	hack_hlr->indirect_table = (uint32_t *)((unsigned long)(hack_hlr->mach_address)
								+ dup_dysymtab.indirectsymoff); 
	hack_hlr->nindirectsyms = dup_dysymtab.nindirectsyms;
	*/

#if DEBUG
	printf("\n\nsymble table info:\n");
	printf("\t\tsymbol_table [file] = %p, ", hack_hlr->symbol_table);
	printf("\t\tnumbers of synmbols = %llx\n", hack_hlr->nsyms);
#endif
	return true;
}

static struct section_64 * load_section_in_vm(struct hack_handler *hack_hlr,
					const char *t_segname, const char *t_sectname)
{

	struct section_64 * ret = NULL;
	uint32_t load_command_count = ((mach_header_64_t)(hack_hlr->library_address))->ncmds;
	struct load_command const * cur_cmd = (struct load_command const *)((char const *)(hack_hlr->library_address) 
												+ sizeof(struct mach_header_64));
	struct load_command dup_cmd = *(cur_cmd);
	if (should_swap)
		swap_load_command(&dup_cmd, 0);

	for (int index = 0; index < load_command_count; 
		index++, cur_cmd = (struct load_command const *)(((char const *)cur_cmd) + dup_cmd.cmdsize)) {

		dup_cmd = *cur_cmd;
		if (should_swap)
			swap_load_command(&dup_cmd, 0);

		switch(dup_cmd.cmd) {
			case LC_SEGMENT_64: {
				struct segment_command_64 * segcmd =  (struct segment_command_64 *)cur_cmd;
				struct segment_command_64 dup_segcmd = *segcmd;
				if (should_swap)
					swap_segment_command_64(&dup_segcmd, 0);

				#if DEBUG
				printf("\t\t%s segment memory address %p\n", dup_segcmd.segname, dup_segcmd.vmaddr);
				#endif

				if (hack_hlr->vm_slide == 0 && !strcmp(dup_segcmd.segname, SEG_TEXT)) {
					hack_hlr->vm_slide = (int64_t)(hack_hlr->library_address - dup_segcmd.vmaddr);
				#if DEBUG
					printf("\t\tvm_slide [loaded_vm - SEG_TEXT_vm] = %p;\n", hack_hlr->vm_slide);
				#endif
				}

				if (!strcmp(dup_segcmd.segname, t_segname)) {
					struct section_64 const * cur_sect = (struct section_64 const *)(segcmd + 1);
					uint32_t nsects = dup_segcmd.nsects;

					for (int secidx = 0; secidx < nsects; secidx++, cur_sect++) {
						#if DEBUG
						printf("\t\tTarget : %s section memory address[vm] %p\n",
							t_segname, cur_sect->addr);
						#endif
						if (!strcmp(cur_sect->sectname, t_sectname)) {
							#if DEBUG
							printf("\t\t%s section size in byte %llu\n", t_sectname, cur_sect->size);
							#endif
							ret = (struct section_64*) cur_sect;
							goto out;
						}
					}
				}
				break;
			}
			default :
				break;
		}
	}
out:
	return ret;
}

static struct section_64 *load_text_section(struct hack_handler *h)
{
	struct section_64 * text_sect = load_section_in_vm(h, SEG_TEXT, SECT_TEXT);
	if (text_sect != NULL) {
		h->text_sect_addr = text_sect->addr;
		h->text_sect_offset = text_sect->offset;
	}
	return text_sect;
}

bool prepare_detour(struct hack_handler *hack_handler_ptr)
{
	//get file load address and vm address
	bool ret = get_dlinfo(dispatch_queue_create, hack_handler_ptr);
	if (ret == false)
		return false;
	//get symbol tables
	ret = get_symbol_tables(hack_handler_ptr);
	if (ret == false)
		return false;
	//get vm_slide
	if (load_text_section(hack_handler_ptr) == NULL)
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
	
	uint64_t ret = hack_hlr->text_sect_addr - hack_hlr->text_sect_offset
					+ hack_hlr->vm_slide
					+ dup_sym.n_value;
	
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
		"\x90" /*nop*/
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
