//
//  maps.h
//  system_cmds
//
//  Created by weng on 2/15/17.
//
//

#ifndef maps_h
#define maps_h
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <libproc.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <mach-o/ldsyms.h>
#include <stdio.h>

const char * get_proc_comm(uint64_t tid);
bool store_proclist(const char * filename);
void load_proclist(const char * filename);
void save_tp_maps(uint64_t tid, pid_t pid);
void update_pid_command(uint64_t tid, const char * command);
void checking_missing_maps(void);

bool get_libinfo(pid_t pid, const char * filename_prefix);

#endif /* maps_h */
