#ifndef ORIGINAL

#define DEBUG 1

#ifdef __cplusplus
extern "C" {
#endif
#include "maps.h"

bool store_proclist(const char * filename)
{
	int bufsize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
	pid_t * procs = (pid_t *)malloc(bufsize * 2);
	bufsize = proc_listpids(PROC_ALL_PIDS, 0, procs, bufsize * 2);

	struct proc_bsdinfo proc;
	int ret;
	FILE * fout = fopen(filename, "wb+"); 
	if (fout == NULL) {
		free(procs);
		#if DEBUG
		printf("Unable to open file  %s for write procs\n", filename);
		#endif
		return false;
	}

	for (int i = 0; i < bufsize / sizeof (pid_t); i++) {
		ret = proc_pidinfo(procs[i], PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
		if (ret == PROC_PIDTBSDINFO_SIZE)
			fprintf(fout, "%d %s\n", procs[i], proc.pbi_comm);
	}
	fclose(fout);

	for (int i = 0; i < bufsize / sizeof (pid_t); i++) {
		get_libinfo(procs[i], filename);
	}
	
	free(procs);
	return true;
}

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
#include <sys/stat.h>

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

unsigned char *
read_task_memory(mach_port_t target_task, mach_vm_address_t addr, mach_msg_type_number_t *size)
{
	mach_msg_type_number_t  data_cnt = (mach_msg_type_number_t) *size;
	vm_offset_t read_mem;
	kern_return_t kr = mach_vm_read(target_task, addr, data_cnt, &read_mem, size);
	if (kr) {
		#if DEBUG
		printf("fail to read task memory in %llx :  %s !\n", addr, mach_error_string(kr));
		#endif
		return NULL;
	}
	return ( (unsigned char *) read_mem);
}

bool get_libinfo(pid_t pid, const char *filename_prefix)
{
	FILE *fp_bout;
	char outfile_path[80] = "./tmp/";
	char proc_path[MAX_PATH] = {0};
	struct proc_bsdinfo proc;
	mach_port_t target_task = MACH_PORT_NULL;
	int ret;
	struct stat st = {0};

	if (stat(outfile_path, &st) == -1)
		mkdir(outfile_path, 0777);

	ret = proc_pidpath(pid, proc_path, MAX_PATH);
	if (ret <= 0) {
		#if DEBUG
		printf("%d : Cannot get proc path\n", pid);
		#endif
		return false;
	}

	ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
	if (ret != PROC_PIDTBSDINFO_SIZE) {
		#if DEBUG
		printf("%d : Cannot get process info\n", pid);
		#endif
		return false;
	}

	ret = task_for_pid(mach_task_self(), pid, &target_task); 
	if (ret != KERN_SUCCESS) {
		#if DEBUG
		printf("task_for_pid() failed with message %s!\n", mach_error_string(ret));
		#endif
		return false;
	}

	kern_return_t kret;
	vm_map_offset_t vm_offset;
	vm_map_size_t vm_size;
	uint32_t nested_depth = 0;
	struct vm_region_submap_info_64 vbr;
	mach_msg_type_number_t vbrcount = 16;

	kret = mach_vm_region_recurse(target_task, &vm_offset, &vm_size,
			&nested_depth, (vm_region_recurse_info_t)&vbr, &vbrcount);
	if (kret != KERN_SUCCESS) {
		#if DEBUG
		printf("%d mach_vm_region_recurse() failed with message %s!\n", pid, mach_error_string(kret));
		#endif
		mach_port_deallocate(mach_task_self(), target_task);
		return false;
    }

	sprintf(outfile_path + strlen(outfile_path), "%s_%d_libinfo", filename_prefix, pid);

	if (!(fp_bout = fopen(outfile_path, "w"))) {
		mach_port_deallocate(mach_task_self(), target_task);
		return false;
	}

	fprintf(fp_bout, "DumpProcess %x %s\n", pid, proc.pbi_comm);
	//fprintf(fp_bout, "%llx %s\n", vm_offset, proc_path);

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

	size = sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount;
	data = read_task_memory(target_task, (mach_vm_address_t)(all_image_info_addr->infoArray), &size);
	struct dyld_image_info * p = (struct dyld_image_info *)data;

	//printf("Dump libs : %llx (size = %lu) /%lu\n", (uint64_t)p, size, sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount);
	
	char * fpath_addr;
	for (int i = 0; i < all_image_info_addr->infoArrayCount; i++, p++) {
		size = MAX_PATH;
		data = read_task_memory(target_task, (mach_vm_address_t)(p->imageFilePath), &size);
		fpath_addr = (char *)data;
		if (fpath_addr == NULL) {
			/*
			if((uint64_t)p->imageLoadAddress == vm_offset)
				continue;
			else
				fprintf(fp_bout, "%llx unable/to/get/path\n", (uint64_t)p->imageLoadAddress);
			*/
			fprintf(fp_bout, "%llx %s\n", (uint64_t)p->imageLoadAddress, proc_path);
		} else {
			fprintf(fp_bout, "%llx %s\n", (uint64_t)p->imageLoadAddress, fpath_addr);
		}
	}

	fflush(fp_bout);
	fclose(fp_bout);
	mach_port_deallocate(mach_task_self(), target_task);
	return true;
}

#ifdef __cplusplus
}
#endif

#include <map>
#include <set>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>

std::map<pid_t, std::string> curprocs;
std::map<uint64_t, pid_t> tpmaps;

/*
extern "C" void load_proclist(const char * filename);
extern "C" const char * get_proc_comm(uint64_t tid);
extern "C" void save_tp_maps(uint64_t tid, pid_t pid);
*/

void load_proclist(const char * filename)
{
#if DEBUG
	std::cout << "Load proc file " << filename << std::endl;
#endif
	std::ifstream fin(filename);
	if (fin.fail())
		return;

#if DEBUG
	std::cout << "read proc file " << filename << std::endl;
#endif
	std::string line;
	
	pid_t pid;
	std::string comm;
	size_t len;

	while (getline(fin, line)) {
		#if DEBUG
		std::cout << "line : " << line << std::endl;
		#endif
		std::istringstream iss(line);
		if (iss >> pid && getline(iss >> std::ws, comm)) {
			if (comm.size() == 0)
				continue;
			if (comm.size() <= MAXCOMLEN)
				curprocs[pid] = comm;
			else
				curprocs[pid] = comm.substr(0, MAXCOMLEN);
			#if DEBUG
			std::cout << "proc[" << std::dec << pid << "] " << curprocs[pid] << std::endl;
			#endif
		}
	}
	#if DEBUG
	std::cout << "Finish proc file load " << filename << std::endl;
	#endif

	fin.close();
}

std::map<uint64_t, std::string> tid_comm;
std::set<uint64_t> missing_tid;
std::set<pid_t> missing_pid;


const char * get_proc_comm(uint64_t tid)
{
	pid_t pid;
	if (tpmaps.find(tid) != tpmaps.end()) {
		pid = tpmaps[tid];
	} else {
		missing_tid.insert(tid);
		#if DEBUG
		std::cout << "unknown pid for thread " << std::hex << tid << std::endl;
		#endif
		return "";
	}
	
	if (tid_comm.size() > 0) {
		std::map<uint64_t, std::string>::iterator it;
		for (it = tid_comm.begin(); it != tid_comm.end(); ){
			if (tpmaps.find(it->first) != tpmaps.end()) {
				curprocs[tpmaps[it->first]] = it->second;
				tid_comm.erase(it++);
			} else {
				it++;
			}
		}
	}
	
	if (curprocs.find(pid) != curprocs.end())
		return (const char *)curprocs[pid].c_str();

	#if DEBUG
	missing_pid.insert(pid);
	std::cout << "unknown command for process " << std::dec << pid  << "checking tid " << std::hex << tid << std::endl;
	#endif

	return "";
}

void save_tp_maps(uint64_t tid, pid_t pid)
{
	tpmaps[tid] = pid;
}

void update_pid_command(uint64_t tid, const char * command)
{
	std::string comm(command);
	tid_comm[tid] = comm;
}

void checking_missing_maps(void)
{
	std::set<uint64_t>::iterator t_it;
	std::set<pid_t>::iterator p_it;
	uint64_t tid;
	pid_t pid;

	for (t_it = missing_tid.begin(); t_it != missing_tid.end(); t_it++) {
		tid = *t_it;
		if (tpmaps.find(tid) == tpmaps.end()) {
			std::cout << "Misding tid 0x" << std::hex << tid << std::endl;
		} else {
			std::cout << "Find pid :" << std::dec << tpmaps[tid] << " for tid 0x" << std::hex << tid << std::endl;
		}
	}

	for (p_it = missing_pid.begin(); p_it != missing_pid.end(); p_it++) {
		pid = *p_it;
		if (curprocs.find(pid) == curprocs.end()) {
			std::cout << "Missing pid " << std::dec << pid << std::endl;
		} else {
			std::cout << "Find command " << curprocs[pid] << " for pid " << std::dec << pid << std::endl;
		}
	}
}

#endif
