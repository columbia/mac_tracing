#ifndef ORIGINAL
#define DEBUG 0
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
			printf("Unable to open file  %s for write procs\n", filename);
			return false;
		}

		for (int i = 0; i < bufsize / sizeof (pid_t); i++) {
			ret = proc_pidinfo(procs[i], PROC_PIDTBSDINFO, 0, &proc,
					PROC_PIDTBSDINFO_SIZE);
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
		read_task_memory(mach_port_t target_task, mach_vm_address_t addr,
				mach_msg_type_number_t *size)
		{
			mach_msg_type_number_t  data_cnt = (mach_msg_type_number_t) *size;
			vm_offset_t read_mem;
			kern_return_t kr = mach_vm_read(target_task, addr, data_cnt,
					&read_mem, size);
			if (kr) {
#if DEBUG
				printf("fail to read task memory in %p :  %s !\n",
						addr, mach_error_string(kr));
#endif
				return NULL;
			}
			return ((unsigned char *) read_mem);
		}

	kern_return_t task_vm_region(mach_port_t target_task,
			pid_t pid, vm_map_offset_t *vm_offset)
	{
		/*
		   vm_map_size_t vm_size;
		   uint32_t nested_depth = 0;
		   struct vm_region_submap_info_64 vbr;
		   mach_msg_type_number_t vbrcount = 16;

		   kern_return_t kret = mach_vm_region_recurse(target_task, vm_offset,
		   &vm_size, &nested_depth,
		   (vm_region_recurse_info_t)&vbr, &vbrcount);

		   if (kret != KERN_SUCCESS) {
		   printf("%d mach_vm_region_recurse() failed with message %s!\n",
		   pid, mach_error_string(kret));
		   }
		   return kret;
		 */
		*vm_offset = 0;
		return KERN_SUCCESS;
	}

	bool get_dyld_info(mach_port_t target_task,
			pid_t pid,
			FILE *fp_bout,
			vm_map_offset_t offset,
			const char *proc_path,
			const char *proc_comm);

	bool get_dyld_info_32(mach_port_t target_task,
			task_dyld_info_data_t info,
			FILE *fp_bout,
			vm_map_offset_t vm_offset,
			const char *proc_path);

	bool get_dyld_info_64(mach_port_t target_task,
			task_dyld_info_data_t info,
			FILE *fp_bout,
			vm_map_offset_t vm_offset,
			const char *proc_path);

	bool get_libinfo(pid_t pid, const char *filename_prefix)
	{
		FILE *fp_bout;
		char * pch = strstr(filename_prefix, ".");
		char outfile_path[80] = "";
		char proc_path[MAX_PATH] = {0};
		struct proc_bsdinfo proc;
		mach_port_t target_task = MACH_PORT_NULL;
		int ret;
		struct stat st = {0};

		if (pch != NULL) {
			strncpy(outfile_path, filename_prefix, pch - filename_prefix);
			strcat(outfile_path, "_libs");
		}
		else
			strcpy(outfile_path, "tmp_libs");

		if (stat(outfile_path, &st) == -1)
			mkdir(outfile_path, 0777);

		ret = proc_pidpath(pid, proc_path, MAX_PATH);
		if (ret <= 0) {
#if DEBUG
			printf("pid[%d] : Cannot get proc path\n", pid);
#endif
			return false;
		}

		ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
		if (ret != PROC_PIDTBSDINFO_SIZE) {
#if DEBUG
			printf("pid[%d] : Cannot get process info\n", pid);
#endif
			return false;
		}

		ret = task_for_pid(mach_task_self(), pid, &target_task); 
		if (ret != KERN_SUCCESS) {
#if DEBUG
			printf("%s : task_for_pid(%d) failed with message %s!\n",
					proc.pbi_comm,
					pid,
					mach_error_string(ret));
#endif
			return false;
		}

		vm_map_offset_t vm_offset;
		kern_return_t kret = task_vm_region(target_task, pid, &vm_offset);
		if (kret != KERN_SUCCESS) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}

		sprintf(outfile_path + strlen(outfile_path),
				"/%s_%d_libinfo", filename_prefix, pid);

		if (!(fp_bout = fopen(outfile_path, "w"))) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}

		//fprintf(fp_bout, "DumpProcess %x %s\t", pid, proc.pbi_comm);
		ret = get_dyld_info(target_task, pid, fp_bout, vm_offset, proc_path, proc.pbi_comm);

		fflush(fp_bout);
		fclose(fp_bout);

		if (ret)
			mach_port_deallocate(mach_task_self(), target_task);
		return ret;
	}

	bool get_dyld_info(mach_port_t target_task,
			pid_t pid,
			FILE *fp_bout,
			vm_map_offset_t offset,
			const char *proc_path,
			const char *proc_comm)
	{
		//get the list of dynamic libraries
		task_dyld_info_data_t info;
		mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
		kern_return_t kret = task_info(target_task, TASK_DYLD_INFO,
				(task_info_t)&info, &count);
		if (kret != KERN_SUCCESS) {
			printf("task_info() fail with message %s!\n", mach_error_string(kret));
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}

		if (info.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_32) {
			fprintf(fp_bout, "DumpProcess %x i386 %s\n", pid, proc_comm);
			return get_dyld_info_32(target_task, info, fp_bout, offset, proc_path);
		} else {
			fprintf(fp_bout, "DumpProcess %x x86_64 %s\n", pid, proc_comm);
			return get_dyld_info_64(target_task, info, fp_bout, offset, proc_path);
		}
	}

	bool get_dyld_info_32(mach_port_t target_task,
			task_dyld_info_data_t task_dyld_info,
			FILE *fp_bout,
			vm_map_offset_t vm_offset,
			const char *proc_path)
	{
		mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos_32);
		struct dyld_all_image_infos_32 *all_image_info_addr \
			= (struct dyld_all_image_infos_32 *)task_dyld_info.all_image_info_addr;
		uint8_t *data = read_task_memory(target_task, 
				(mach_vm_address_t)all_image_info_addr, &size);

		if (!data) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}
		all_image_info_addr = (struct dyld_all_image_infos_32 *)data;

#if DEBUG
		printf("Dump %s all lib info : %p (size = %lu/%lu)\n",
				proc_path,
				(uint32_t)(task_dyld_info.all_image_info_addr),
				size,
				sizeof(struct dyld_all_image_infos_32));
#endif

		size = sizeof(struct dyld_image_info_32) *\
			   all_image_info_addr->infoArrayCount;

#if DEBUG
		printf("libinfo array size = %d begins at 0x%x\n",
				all_image_info_addr->infoArrayCount,
				(uint32_t)all_image_info_addr->infoArray);
#endif

		data = read_task_memory(target_task,
				(mach_vm_address_t)(all_image_info_addr->infoArray), &size);
		if (data == NULL) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}
		struct dyld_image_info_32 *p = (struct dyld_image_info_32 *)data;

#if DEBUG
		printf("Dump libs : %p (size = %lu/%lu)\n",
				(uintptr_t)p,
				size,
				sizeof(struct dyld_image_info_32) * all_image_info_addr->infoArrayCount);
#endif

		char * fpath_addr;
		for (int i = 0; i < all_image_info_addr->infoArrayCount; i++, p++) {
			size = MAX_PATH;
			data = read_task_memory(target_task,
					(mach_vm_address_t)(p->imageFilePath), &size);
			fpath_addr = (char *)data;
			if (fpath_addr == NULL) {
				/* uggly makeup the address read from structure for App
				 * not accessible
				 * TODO : figure out why
				 */
#if DEBUG
				fprintf(fp_bout, "0x%x unable/to/get/path\n",
						(uintptr_t)p->imageLoadAddress);
#endif

				fprintf(fp_bout, "0x%x %s\n", p->imageLoadAddress, proc_path);
			} else {
				fprintf(fp_bout, "0x%x %s\n", p->imageLoadAddress, fpath_addr);
			}
		}
		return true;
	}

	bool get_dyld_info_64(mach_port_t target_task,
			task_dyld_info_data_t info,
			FILE *fp_bout,
			vm_map_offset_t vm_offset,
			const char *proc_path)
	{
		mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos);
		struct dyld_all_image_infos *all_image_info_addr \
			= (struct dyld_all_image_infos *)info.all_image_info_addr;
		uint8_t *data = read_task_memory(target_task, 
				(mach_vm_address_t)all_image_info_addr, &size);

		if (!data) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}
		all_image_info_addr = (struct dyld_all_image_infos *)data;

#if DEBUG
		printf("Dump %s all lib info : %p (size = %lu/%lu)\n",
				proc_path,
				(uintptr_t)(info.all_image_info_addr),
				size,
				sizeof(struct dyld_all_image_infos));
#endif

		size = sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount;

#if DEBUG
		printf("libinfo array size = %d begins at %p\n",
				all_image_info_addr->infoArrayCount,
				all_image_info_addr->infoArray);
#endif

		data = read_task_memory(target_task,
				(mach_vm_address_t)(all_image_info_addr->infoArray), &size);
		if (data == NULL) {
			mach_port_deallocate(mach_task_self(), target_task);
			return false;
		}
		struct dyld_image_info *p = (struct dyld_image_info *)data;

#if DEBUG
		printf("Dump libs : %p (size = %lu/%lu)\n",
				(uintptr_t)p,
				size,
				sizeof(struct dyld_image_info) * all_image_info_addr->infoArrayCount);
#endif

		char * fpath_addr;
		for (int i = 0; i < all_image_info_addr->infoArrayCount; i++, p++) {
			size = MAX_PATH;
			data = read_task_memory(target_task,
					(mach_vm_address_t)(p->imageFilePath), &size);
			fpath_addr = (char *)data;
			if (fpath_addr == NULL) {
				/* uggly makeup the address read from structure for App
				 * not accessible
				 * TODO : figure out why
				 */
#if DEBUG
				fprintf(fp_bout, "%p unable/to/get/path\n",
						(uintptr_t)p->imageLoadAddress);
#endif

				fprintf(fp_bout, "%p %s\n", p->imageLoadAddress, proc_path);
			} else {
				fprintf(fp_bout, "%p %s\n", p->imageLoadAddress, fpath_addr);
			}
		}
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

std::set<uint64_t> missing_tid;
std::set<pid_t> missing_pid;

std::map<pid_t, std::string> pcmaps;
std::map<uint64_t, std::string> tcmaps;
std::map<uint64_t, pid_t> tpmaps;
std::map<uint64_t, std::pair<pid_t, std::string> > tpcmaps;

void load_proclist(const char *filename)
{
	std::cout << "Load proc file " << filename << std::endl;
	std::ifstream fin(filename);
	if (fin.fail())
		return;

	std::string line;
	pid_t pid;
	std::string comm;

	while (getline(fin, line)) {
#if DEBUG
		std::cout << "line : " << line << std::endl;
#endif
		std::istringstream iss(line);
		if (iss >> pid && getline(iss >> std::ws, comm)) {
			if (comm.size() == 0)
				continue;
			if (comm.size() <= MAXCOMLEN)
				pcmaps[pid] = comm;
			else
				pcmaps[pid] = comm.substr(0, MAXCOMLEN);
#if DEBUG
			std::cout << "proc[" << std::dec << pid << "] "\
				<< pcmaps[pid] << std::endl;
#endif
		}
	}
	std::cout << "Finish loading proc file " << filename << std::endl;

	fin.close();
}
// called when a tracing point misses command in its line
const char *get_proc_comm(uint64_t tid)
{
	pid_t pid = -1;
	std::string comm = "";

	if (tpcmaps.find(tid) != tpcmaps.end()) {
		tpmaps.erase(tid);
		tcmaps.erase(tid);
		return (tpcmaps[tid]).second.c_str();
	}

	if (tpmaps.find(tid) != tpmaps.end())
		pid = tpmaps[tid];

	if (tcmaps.find(tid) != tcmaps.end())
		comm = tcmaps[tid];


	/*if pid found try to sync it to tpcmaps*/
	if (pid != -1 && pcmaps.find(pid) != pcmaps.end()) {
		tpcmaps[tid] = std::make_pair(pid, pcmaps[pid]);
		tpmaps.erase(tid);
		tcmaps.erase(tid);
		return pcmaps[pid].c_str();
	}

	/*sync for threads in tcmaps*/
	std::map<uint64_t, std::string>::iterator it;
	for (it = tcmaps.begin(); it != tcmaps.end();) {
		if (tpmaps.find(it->first) != tpmaps.end()) {
			pcmaps[tpmaps[it->first]] = it->second;
			tpcmaps[it->first] = std::make_pair(tpmaps[it->first], tcmaps[it->first]);
			tpmaps.erase(it->first);
			tcmaps.erase(it++);
			continue;
		}
		it++;
	}

	if (tpcmaps.find(tid) != tpcmaps.end())
		return (tpcmaps[tid]).second.c_str();

	missing_tid.insert(tid);
#if DEBUG
	std::cout << "missing map for thread " << std::hex << tid << std::endl;
#endif

	if (pid != -1 && pcmaps.find(pid) == pcmaps.end()) {
		missing_pid.insert(pid);
#if DEBUG
		std::cout << "missing command for pid " << std::dec << pid << std::endl;
		std::cout << "checking tid " << std::hex << tid << std::endl;
#endif
	}

	return comm.c_str();
}

void save_tp_maps(uint64_t tid, pid_t pid)
{
	tpmaps[tid] = pid;
}

void update_tc_map(uint64_t tid, const char *command)
{
	std::string comm(command);
	tcmaps[tid] = comm;
}

void checking_missing_maps(void)
{
	std::set<uint64_t>::iterator t_it;
	std::set<pid_t>::iterator p_it;
	uint64_t tid;
	pid_t pid;
	bool missing = false;

	for (t_it = missing_tid.begin(); t_it != missing_tid.end(); t_it++) {
		tid = *t_it;
		if (tpcmaps.find(tid) == tpcmaps.end() && tpmaps.find(tid) == tpmaps.end()) {
			std::cout << "Missing tid 0x" << std::hex << tid << std::endl;
			missing = true;
		}
	}

	for (p_it = missing_pid.begin(); p_it != missing_pid.end(); p_it++) {
		pid = *p_it;
		std::map<uint64_t, std::pair<pid_t, std::string> >::iterator it;
		for (it = tpcmaps.begin(); it != tpcmaps.end(); it++)
			if ((it->second).first == pid)
				goto next;
		if (pcmaps.find(pid) == pcmaps.end()) {
			std::cout << "Missing pid " << std::dec << pid << std::endl;
			missing = true;
		}
next:;
	}

	if (missing == false)
		std::cout << "No missing info" << std::endl;

}

void sync_tpc_maps(const char * logfile)
{
	std::cout << "store maps for " << logfile << ": " << std::endl;
	std::string outfile(logfile);
	outfile = outfile + "_tpcmaps";
	std::ofstream output(outfile);
	if (output.fail())
		return;

	std::cout << "open file " << outfile << " for write" << std::endl;
	std::map<uint64_t, pid_t>::iterator it;
	uint64_t tid;
	pid_t pid;
	int count = 0;
	for (it = tpmaps.begin(); it != tpmaps.end(); it++) {
		tid = it->first;
		pid = it->second;
		if (pcmaps.find(pid) != pcmaps.end()) {
			output << std::hex << tid << "\t" << std::hex << pid << "\t" << pcmaps[pid] << std::endl;
			count++;
			//tpcmaps[tid] = std::mkpair(pid, pcmaps[pid]);
		}
	}

	std::map<uint64_t, std::pair<pid_t, std::string> >::iterator it_2;
	for (it_2 = tpcmaps.begin(); it_2 != tpcmaps.end(); it_2++) {
		output << std::hex << it_2->first << "\t" << std::hex << it_2->second.first << "\t" << it_2->second.second << std::endl;
		count++;
	}

	std::cout << "#" << count << " items are stored" << std::endl; 
	output.close();
}
#endif
