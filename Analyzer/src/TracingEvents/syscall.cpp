#include "syscall.hpp"

SyscallEvent::SyscallEvent(double timestamp, string op, uint64_t tid, uint64_t sc_class, uint32_t event_core, string procname)
:EventBase(timestamp, op, tid, event_core, procname)
{
	syscall_class = sc_class;
	sc_entry = NULL;
	memset(args, sizeof(uint64_t) * MAX_ARGC, 0);
}

void SyscallEvent::set_args(uint64_t *arg_array, uint64_t size)
{
	assert(size == 4);
	copy(arg_array, arg_array + size, args);
}

uint64_t SyscallEvent::get_arg(int idx)
{
	if (idx < MAX_ARGC)
		return args[idx];
	return 0;
}

const char *SyscallEvent::class_name()
{
	switch(syscall_class) {
		case MSC_SYSCALL:
			return "mach_syscall";
		case BSC_SYSCALL:
			return "bsd_syscall";
		default:
			return "unknown_syscall";
	}
}

void SyscallEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "\t" << class_name();
	if (sc_entry != NULL) {
		outfile << "\n\t" << sc_entry->syscall_name;
		for (int i = 0; i < MAX_ARGC; i++) {
			if (sc_entry->args[i] != NULL)
				outfile << "\n\t" << sc_entry->args[i] << " = " << args[i];
		}
	}
	outfile << "\n\treturn = " << hex << ret;
	outfile << endl;
}

void SyscallEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << hex << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "\t" << hex << ret;
	outfile << endl;
}
