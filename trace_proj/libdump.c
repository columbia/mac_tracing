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

#define MAX_PATH 2048

unsigned char *
read_task_memory(mach_port_t target_task, mach_vm_address_t addr, mach_msg_type_number_t *size)
{
	mach_msg_type_number_t  data_cnt = (mach_msg_type_number_t) *size;
	vm_offset_t read_mem;
	kern_return_t kr = mach_vm_read(target_task, addr, data_cnt, &read_mem, size);
	if (kr) {
		printf("fail to read task memory in %llx :  %s !\n", addr, mach_error_string(kr));
		return NULL;
	}
	return ( (unsigned char *) read_mem);
}

bool get_libinfo(pid_t pid)
{
	FILE *fp_bout;
	char outfile_path[64] = "/tmp/";
	char proc_path[MAX_PATH] = {0};
	struct proc_bsdinfo proc;
	mach_port_t target_task = MACH_PORT_NULL;
	int ret;

	ret = proc_pidpath(pid, proc_path, MAX_PATH);
	if (ret <= 0) {
		printf("Cannot get proc path\n");
		return false;
	}

	ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
	if (ret != PROC_PIDTBSDINFO_SIZE) {
		printf("Cannot get process info\n");
		return false;
	}

	ret = task_for_pid(mach_task_self(), pid, &target_task); 
	if (ret != KERN_SUCCESS) {
		printf("task_for_pid() failed with message %s!\n", mach_error_string(ret));
		return false;
	}

	//get vm_slice of the executable
	kern_return_t kret;
	vm_map_offset_t vm_offset;
	vm_map_size_t vm_size;
	uint32_t nested_depth = 0;
	struct vm_region_submap_info_64 vbr;
	mach_msg_type_number_t vbrcount = 16;

	kret = mach_vm_region_recurse(target_task, &vm_offset, &vm_size,
			&nested_depth, (vm_region_recurse_info_t)&vbr, &vbrcount);
	if (kret != KERN_SUCCESS) {
		printf("mach_vm_region_recurse() failed with message %s!\n", mach_error_string(kret));
		mach_port_deallocate(mach_task_self(), target_task);
		return false;
    }
	sprintf(outfile_path + strlen(outfile_path), "%d_libinfo.test", pid);
	if (!(fp_bout = fopen(outfile_path, "w"))) {
		mach_port_deallocate(mach_task_self(), target_task);
		return false;
	}

	fprintf(fp_bout, "DumpProcess %x %s\n", pid, proc.pbi_comm);
	fprintf(fp_bout, "%llx %s\n", vm_offset, proc_path);

	//get the list of dynamic libraries
	task_dyld_info_data_t info;
	mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
	struct dyld_all_image_infos * all_image_info_addr;
	
	kret = task_info(target_task, TASK_DYLD_INFO, (task_info_t)&info, &count);
	if (kret != KERN_SUCCESS) {
		printf("task_info() failed with message %s!\n", mach_error_string(kret));
		fclose(fp_bout);
		mach_port_deallocate(mach_task_self(), target_task);
		return false;
	}

	mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos);
	all_image_info_addr = (struct dyld_all_image_infos *)info.all_image_info_addr;
	uint8_t* data = read_task_memory(target_task, (mach_vm_address_t)all_image_info_addr, &size);
	all_image_info_addr = (struct dyld_all_image_infos *)data;

	//printf("Dump all lib info : %llx (size = %lu/%d)\n", (uint64_t)(info.all_image_info_addr), sizeof(struct dyld_all_image_infos));

	struct dyld_image_info * p = all_image_info_addr->infoArray;
	size = sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount;
	data = read_task_memory(target_task, p, &size);
	p = (struct dyld_image_info *)data;

	//printf("Dump libs : %llx (size = %lu) /%lu\n", (uint64_t)p, size, sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount);
	
	char * fpath_addr;
	for (int i = 0; i < all_image_info_addr->infoArrayCount; i++, p++) {
		size = MAX_PATH;
		data = read_task_memory(target_task, (mach_vm_address_t)(p->imageFilePath), &size);
		fpath_addr = (char *)data;
		if (fpath_addr == NULL) {
			if((uint64_t)p->imageLoadAddress == vm_offset)
				continue;
			else
				fprintf(fp_bout, "%llx unable/to/get/path\n", (uint64_t)p->imageLoadAddress);
		} else {
			fprintf(fp_bout, "%llx %s\n", (uint64_t)p->imageLoadAddress, fpath_addr);
		}
	}

	fflush(fp_bout);
	fclose(fp_bout);
	mach_port_deallocate(mach_task_self(), target_task);
	return true;
}

int  main(int argc, char *argv[])
{
	if (argc < 2)
		return 0;
	pid_t pid = atoi(argv[1]);
	if (pid <= 0)
		return 0;
	bool ret = get_libinfo(pid);
	return 0;
}
