#ifndef LOADER_HPP
#define LOADER_HPP
#include "base.hpp"
#include "syscall.hpp"

#define MACH_IPC_MSG				1
#define MACH_IPC_VOUCHER_INFO		2
#define MACH_IPC_VOUCHER_CONN		3
#define MACH_IPC_VOUCHER_TRANSIT	5
#define MACH_IPC_VOUCHER_DEALLOC	6
#define MACH_BANK_ACCOUNT			7
#define MACH_MK_RUN					8
#define INTR						9
#define WQ_NEXT						10
#define MACH_TS						11
#define MACH_WAIT					12
#define DISP_ENQ					13
#define DISP_DEQ					14
#define DISP_EXE					15
#define MACH_CALLCREATE				16
#define MACH_CALLOUT				17
#define MACH_CALLCANCEL				18
#define BACKTRACE					19
//#define BT_PATH					
//#define BT_FRAME					
#define MACH_SYS					20
#define BSD_SYS						21
#define CA_SET						22
#define CA_DISPLAY					23
#define BREAKPOINT_TRAP					24
#define RL_OBSERVER					25
#define EVENTREF					26
#define NSAPPEVENT					27

#define MAX_ARGC 16
struct syscall_entry {
	int64_t syscall_number;
	const char *syscall_name;
	const char *args[MAX_ARGC];
};

struct mig_service {
	int mig_num;
	const char *mig_name;
};

namespace LoadData
{
	typedef struct Meta{
		string datafile;
		string libinfo_file;
		string procs_file;
		string intersectfile;
		string tpc_maps_file;
		string host;
		string host_arch;
		pid_t pid;
		string suspicious_api;
		double spin_timestamp;
		uint64_t nthreads;
	} meta_data_t;
	extern meta_data_t meta_data;

	extern map<uint64_t, pair<pid_t, string> > tpc_maps;
	extern map<int, string> mig_dictionary;
	extern const struct mig_service mig_table[];
	extern const uint64_t mig_size;

	extern map<string, uint64_t> msc_name_index_map;
	extern const struct syscall_entry mach_syscall_table[];
	extern const uint64_t msc_size;

	extern map<string, uint64_t> bsc_name_index_map;
	extern const struct syscall_entry bsd_syscall_table[];
	extern const uint64_t bsc_size;

	extern const map<string, uint64_t> op_code_map;
	uint64_t map_op_code(uint64_t unused, string opname);
	void preload();
}
#endif
