#ifndef SYSCALL_HPP
#define SYSCALL_HPP
#include "base.hpp"
#include "loader.hpp"

#define MSC_SYSCALL 0
#define BSC_SYSCALL 1
#define MAX_ARGC 16

//struct syscall_entry {
//	int64_t syscall_number;
//	const char* syscall_name;
//	const char* args[MAX_ARGC];
//};

/*define in XXXsyscalldef.cpp*/
extern const struct syscall_entry mach_syscall_table[];
extern const struct syscall_entry bsd_syscall_table[];
extern uint64_t msc_size;
extern uint64_t bsc_size;

class SyscallEvent : public EventBase {
	double ret_time;
	const struct syscall_entry * sc_entry;
	uint64_t args[MAX_ARGC];
	uint64_t syscall_class;
	uint64_t ret;
public :
	SyscallEvent(double abstime, string op, uint64_t _tid, uint64_t sc_class, uint32_t event_core, string proc="");
	void set_args(uint64_t * array, uint64_t size);
	uint64_t get_arg(int idx);
	void set_entry(const struct syscall_entry * entry) { sc_entry = entry;}
	const struct syscall_entry * get_entry(void) {return sc_entry;}
	void set_ret(uint64_t _ret) {ret = _ret;}
	void set_ret_time(double time) {ret_time = time;}
	double get_ret_time(void) {return ret_time;}
	uint64_t get_ret(void) {return ret;}
	const char *class_name(void); 
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
