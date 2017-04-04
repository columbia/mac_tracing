#include "mysymbolicator.h"
#include <assert.h>
#define DEBUG 0
#define MAX_PATH 2048
typedef struct mach_header_64 * mach_header_64_t;
typedef struct fat_header * fat_header_t;
typedef struct fat_arch * fat_arch_t;
static bool should_swap = true;
//struct vm_symbol * symbol_list_head = NULL;

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

unsigned char * read_task_memory(mach_port_t task, mach_vm_address_t addr, mach_msg_type_number_t *size)
{
	mach_msg_type_number_t  data_cnt = (mach_msg_type_number_t) *size;
	vm_offset_t read_mem;
	kern_return_t kr = mach_vm_read(task, addr, data_cnt, &read_mem, size);
	if (kr) {
#if DEBUG
		printf("fail to read task memory in %llx :  %s !\n", addr, mach_error_string(kr));
#endif
		return NULL;
	}
	return ( (unsigned char *) read_mem);
}


static mach_header_64_t load_mach_64_from_fat(mach_port_t target_task, void * obj, bool should_swap)
{
	int header_size = sizeof(struct fat_header);
	int arch_size = sizeof(struct fat_arch);
	mach_msg_type_number_t size = header_size;
	kern_return_t kret;
	uint8_t* data  = read_task_memory(target_task, (mach_vm_address_t)obj, &size);
	if (data == NULL)
		return NULL;

	struct fat_header dup_header = *((fat_header_t)data);

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	if (should_swap)
		swap_fat_header(&dup_header, 0);

	int arch_offset = header_size;
	for (int i = 0; i < dup_header.nfat_arch; i++) {
		fat_arch_t arch = (fat_arch_t)(obj + arch_offset);
		size = arch_size;
		data  = read_task_memory(target_task, (mach_vm_address_t)arch, &size);
		if (data == NULL)
			return NULL;
		struct fat_arch dup_arch = *((fat_arch_t)data);

		kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
		if (kret != KERN_SUCCESS) {
#if DEBUG
			printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
		}

		if (should_swap)
			swap_fat_arch(&dup_arch, 1, 0);

		int mach_header_offset = dup_arch.offset;
		arch_offset += arch_size;

		size = 4;
		data  = read_task_memory(target_task, (mach_vm_address_t)(obj + mach_header_offset), &size);
		if (data == NULL)
			return NULL;

		uint32_t magic = *((uint32_t *)(data));
		kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
		if (kret != KERN_SUCCESS) {
#if DEBUG
			printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
		}

		if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
			return (mach_header_64_t)(obj + mach_header_offset);
		}
	}
	return NULL;
}

static void *get_mach(mach_port_t target_task, void *obj)
{
	if (obj == NULL) 
		return NULL;

	mach_msg_type_number_t size = 4;
	uint8_t* data  = read_task_memory(target_task, (mach_vm_address_t)obj, &size);

	if (data == NULL)
		return NULL;

	uint32_t magic = *((uint32_t*)data);

	kern_return_t kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	mach_header_64_t img = NULL;

	if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
		should_swap = fat_need_swap(magic);
		img = load_mach_64_from_fat(target_task, obj, should_swap);
	} 
	else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
		should_swap = mach_need_swap(magic);
		img = (struct mach_header_64 *)obj;
	}

	if(img == NULL) {
		return NULL;
	}

	size = 4;
	data  = read_task_memory(target_task, (mach_vm_address_t)img, &size);
	if (data == NULL)
		return NULL;

	magic = *((uint32_t *)(data));
	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	should_swap = mach_need_swap(magic);

	if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
		return (void *)img;
	} else
		return NULL;
}

static bool load_segments(mach_port_t target_task, struct hack_handler *hack_hlr, struct hack_segments * segments_info)
{
	mach_msg_type_number_t size = sizeof(struct mach_header_64);
	uint8_t* data  = read_task_memory(target_task, (mach_vm_address_t)(hack_hlr->mach_address), &size);
	if (data == NULL)
		return false;

	struct mach_header_64 dup_image = *((mach_header_64_t)data);

	kern_return_t kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	struct load_command *cmd = (struct load_command*)((mach_header_64_t)(hack_hlr->mach_address) + 1);
	struct load_command dup_cmd;

	if (should_swap)
		swap_mach_header_64(&dup_image, 0);

	for (int index = 0; index < dup_image.ncmds; 
			index++, cmd = (struct load_command*)((void *)cmd + dup_cmd.cmdsize)) { 
		size = sizeof(struct load_command);
		data  = read_task_memory(target_task, (mach_vm_address_t)(cmd), &size);
		if (data == NULL)
			return false;
		dup_cmd = *((struct load_command*)data);

		kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
		if (kret != KERN_SUCCESS) {
#if DEBUG
			printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
		}

		if (should_swap)
			swap_load_command(&dup_cmd, 0);

		switch(dup_cmd.cmd) {
			case LC_SEGMENT_64: {
									struct segment_command_64* segcmd = (struct segment_command_64*)cmd;
									size = sizeof(struct segment_command_64);
									data  = read_task_memory(target_task, (mach_vm_address_t)(cmd), &size);
									if (data == NULL)
										return false;

									struct segment_command_64 dup_segcmd = *((struct segment_command_64*)data);
									if (should_swap)
										swap_segment_command_64(&dup_segcmd, 0);
									kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
									if (kret != KERN_SUCCESS) {
#if DEBUG
										printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
									}

#if DEBUG
									printf("\n%s segment %p: vm_addr %p", dup_segcmd.segname, cmd, dup_segcmd.vmaddr);
									printf(", file_off %p", dup_segcmd.fileoff);
									printf(", vm_size %p", dup_segcmd.vmsize);
									printf(", seg size %x\n", dup_segcmd.cmdsize);
#endif

									if (!strcmp(dup_segcmd.segname, SEG_TEXT)) {
										segments_info->seg_text_ptr = segcmd;
										hack_hlr->vm_slide = (int64_t)(hack_hlr->library_address - dup_segcmd.vmaddr);
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
	printf("\nsymbol table related segment info [vm]:\n\t\tsymtab = %p;\n\t\tdysymtab = %p;\n\t\tseg_linkedit = %p;\n",
			segments_info->symtab_ptr, segments_info->dysymtab_ptr, segments_info->seg_linkedit_ptr);
#endif

	if (segments_info->symtab_ptr == NULL)
		return false;
	return true;
}

static bool get_symbol_tables(mach_port_t target_task, struct hack_handler *hack_hlr)
{
	struct hack_segments segments_info = {0, 0, 0, 0};
	bool ret = load_segments(target_task, hack_hlr, &segments_info);
	if (ret == false)
		return false;
	kern_return_t kret;

	mach_msg_type_number_t size = sizeof(struct symtab_command);
	uint8_t* data  = read_task_memory(target_task, (mach_vm_address_t)(segments_info.symtab_ptr), &size);
	if (data == NULL)
		return false;
	struct symtab_command dup_symtab = *((struct symtab_command *)data);
	if (should_swap)
		swap_symtab_command(&dup_symtab, 0);

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	size = sizeof(struct segment_command_64);
	data = read_task_memory(target_task, (mach_vm_address_t)(segments_info.seg_linkedit_ptr), &size);
	struct segment_command_64 dup_linkedit = *((struct segment_command_64 *)data);
	if (should_swap)
		swap_segment_command_64(&dup_linkedit, 0);

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

#if DEBUG
	printf("\n\nlc_symtab(%u) %u\n", LC_SYMTAB, dup_symtab.cmd);
	printf("\t\tcmd_size = %u\n", dup_symtab.cmdsize);
	printf("\t\tsymoff = %p\n", dup_symtab.symoff);
	printf("\t\tstroff = %p\n", dup_symtab.stroff);
	printf("\t\tstrsize = %u\n", dup_symtab.strsize);
	printf("\nlink_edit\n\t\tvmaddr = %p\n", dup_linkedit.vmaddr);
	printf("\t\tfileoff = %p\n", dup_linkedit.fileoff);
#endif

	char * linkEditBase = (char *)(dup_linkedit.vmaddr - dup_linkedit.fileoff + hack_hlr->vm_slide);
	struct nlist_64 *symbols = (struct nlist_64 *)((unsigned long)(linkEditBase) + dup_symtab.symoff);
	char *strings = (char *)((unsigned long)(linkEditBase) + dup_symtab.stroff);

	hack_hlr->string_table = strings;
	hack_hlr->symbol_table = symbols;
	hack_hlr->nsyms = dup_symtab.nsyms;
	hack_hlr->strsize = dup_symtab.strsize;

#if DEBUG
	printf("\n\nsymble table info:\n");
	printf("\t\tsymbol_table [vm] = %p\n", hack_hlr->symbol_table);
	printf("\t\tstring_table [vm] = %p\n", hack_hlr->string_table);
	printf("\t\tnumbers of symbols = %x\n", hack_hlr->nsyms);
	printf("\t\tsize of sym string = %x\n", hack_hlr->strsize);
#endif
	return true;
}

#define FUNC 0
#define LOAD_ADDR 1
bool prepare_symbolication(mach_port_t target_task, struct hack_handler *hack_handler_ptr, void * addr, uint32_t type)
{
	bool ret;
	if (type == FUNC) {
		// Not implemented
		return false;
	}

	if (type == LOAD_ADDR) {
		hack_handler_ptr->library_address = addr;
		hack_handler_ptr->mach_address = get_mach(target_task, addr);
		if (hack_handler_ptr->mach_address == NULL)
			return false;
	}

	//get symbol tables
	ret = get_symbol_tables(target_task, hack_handler_ptr);
	if (ret == false)
		return false;
	return true;
}

int64_t get_local_syms_in_vm(mach_port_t target_task, struct hack_handler *hack_hlr)
{
	struct nlist_64 *symbase = (struct nlist_64 *)hack_hlr->symbol_table;
	struct nlist_64 *sym = symbase;

#if DEBUG
	printf("Read string table\n");
#endif

	mach_msg_type_number_t str_size = hack_hlr->strsize;
	uint8_t* data = read_task_memory(target_task, (mach_vm_address_t)(hack_hlr->string_table), &str_size);
	if (data == NULL)
		return -1;
	hack_hlr->strings = (char *)malloc(str_size + 1);
	//const char * strings = (const char *)data;
	memcpy((void *)(hack_hlr->strings), (const char *)data, str_size);
	hack_hlr->strsize = str_size + 1;
	kern_return_t kret;

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, str_size);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	hack_hlr->symbol_arrays =(struct vm_sym *)malloc(hack_hlr->nsyms * sizeof(struct vm_sym));

	struct nlist_64 dup_sym;
	uint64_t cur_vm_offset = 0;
	uint64_t cur_str_index = 0;
	mach_msg_type_number_t size;

	for (int64_t index = 0; index < hack_hlr->nsyms; index++, sym++) {
#if DEBUG
		printf("Read symbol table entry %d\n", index);
#endif
		size = sizeof(struct nlist_64);
		data = read_task_memory(target_task, (mach_vm_address_t)sym, &size);
		if (data == NULL)
			return -1;
		dup_sym = *((struct nlist_64*)data);
		if (should_swap)
			swap_nlist_64(&dup_sym, 1, 0);

		kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data, size);
		if (kret != KERN_SUCCESS) {
#if DEBUG
			printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
		}

		cur_vm_offset = hack_hlr->vm_slide + dup_sym.n_value - (uint64_t)hack_hlr->mach_address;
		cur_str_index = dup_sym.n_un.n_strx;
		hack_hlr->symbol_arrays[index].vm_offset = cur_vm_offset;
		hack_hlr->symbol_arrays[index].str_index = cur_str_index;
		if (cur_vm_offset == hack_hlr->vm_slide - (uint64_t)hack_hlr->mach_address)
			break;
	}

	return 0;
}

static uint64_t get_lib_load_addr(mach_port_t target_task, const char * path)
{
	//get the list of dynamic libraries
	task_dyld_info_data_t info;
	mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
	struct dyld_all_image_infos * all_image_info_addr;

	kern_return_t kret = task_info(target_task, TASK_DYLD_INFO, (task_info_t)&info, &count);
	if (kret != KERN_SUCCESS) {
		printf("task_info() failed with message %s!\n", mach_error_string(kret));
		return false;
	}

	mach_msg_type_number_t size_1 = sizeof(struct dyld_all_image_infos);
	all_image_info_addr = (struct dyld_all_image_infos *)info.all_image_info_addr;
	uint8_t* data_1 = read_task_memory(target_task, (mach_vm_address_t)all_image_info_addr, &size_1);
	all_image_info_addr = (struct dyld_all_image_infos *)data_1;

#if DEBUG
	printf("Dump all lib info : %llx (size = %lu/%d)\n", (uint64_t)(info.all_image_info_addr), sizeof(struct dyld_all_image_infos));
#endif
	const struct dyld_image_info * p = all_image_info_addr->infoArray;
	mach_msg_type_number_t size_2 = sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount;
	uint8_t* data_2 = read_task_memory(target_task, (mach_vm_address_t)p, &size_2);
	p = (struct dyld_image_info *)data_2;

#if DEBUG
	printf("Dump libs : %llx (size = %lu) /%lu\n", (uint64_t)p, size, sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount);
#endif

	char * fpath_addr;
	uint64_t load_addr_ret = 0;
	for (int i = 0; i < all_image_info_addr->infoArrayCount; i++, p++) {
		mach_msg_type_number_t size_3 = MAX_PATH;
		uint8_t * data_3 = read_task_memory(target_task, (mach_vm_address_t)(p->imageFilePath), &size_3);
		fpath_addr = (char *)data_3;

		if (fpath_addr != NULL) {
#if DEBUG
			printf("%llx %s\n", (uint64_t)p->imageLoadAddress, fpath_addr);
			printf("target_path %s\n", path);
#endif
			if (!strcmp(path, fpath_addr))
				load_addr_ret = (uint64_t)p->imageLoadAddress;

			kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data_3, size_3);
			if (kret != KERN_SUCCESS) {
#if DEBUG
				printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
			}

			if (load_addr_ret != 0)
				break;
		}
	}

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data_2, size_2);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	kret = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)data_1, size_1);
	if (kret != KERN_SUCCESS) {
#if DEBUG
		printf("fail to deallocate task memory in %p, %s !\n", (void *)data, mach_error_string(kret));
#endif
	}

	return load_addr_ret;
}

bool get_syms_for_libpath(pid_t pid, const char *path, struct hack_handler * hack_hlr_ptr)
{
	mach_port_t target_task = MACH_PORT_NULL;
	kern_return_t kret = task_for_pid(mach_task_self(), pid, &target_task);
	bool ret = false;

	if (kret != KERN_SUCCESS) {
		printf("task_for_pid(%d) for symbolicate %s failed with message %s!\n", pid, path, mach_error_string(kret));
		return false;
	}

	uint64_t addr = get_lib_load_addr(target_task, path);
#if DEBUG
	printf ("path %s addr %p\n", path, addr);
#endif
	if (addr != 0) {
		prepare_symbolication(target_task, hack_hlr_ptr, (void *)addr, LOAD_ADDR);
#if DEBUG
		printf("check local sym\n");
#endif
		if (!get_local_syms_in_vm(target_task, hack_hlr_ptr))
			ret = true;
		mach_port_deallocate(mach_task_self(), target_task);
	}
	return ret;
}

/*
const char * get_sym_for_addr(pid_t pid, uint64_t vmoffset, const char * path)
{
	struct hack_handler hack_hlr;
	uint64_t addr;
	mach_port_t target_task = MACH_PORT_NULL;
	kern_return_t kret = task_for_pid(mach_task_self(), pid, &target_task);
	char symbol[512] = "unknown";

	if (kret != KERN_SUCCESS) {
		printf("task_for_pid() failed with message %s!\n", mach_error_string(kret));
		return false;
	}

	addr = get_lib_load_addr(target_task, path);

	#if DEBUG
	printf ("path %s addr %p\n", path, addr);
	#endif

	if (addr != 0) {
		prepare_symbolication(target_task, &hack_hlr, (void *)addr, LOAD_ADDR);
	#if DEBUG
		printf("check local sym\n");
	#endif
		addr = get_local_sym_in_vm(target_task, &hack_hlr, vmoffset, symbol);
		mach_port_deallocate(mach_task_self(), target_task);
		printf("%s\n[%p]\t%s\n", path, vmoffset, symbol);
	} else {
		printf("not a lib %s\n", path);
	}

	#if DEBUG
	printf("vmaddr %p, %s\n", vmoffset, symbol);
	#endif
	return strdup(symbol);
}
*/
