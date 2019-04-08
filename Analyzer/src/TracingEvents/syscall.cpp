#include "syscall.hpp"
#define DEBUG_SYSCALL_EVT 0

SyscallEvent::SyscallEvent(double timestamp, string op, uint64_t tid, uint64_t sc_class, uint32_t event_core, string procname)
:EventBase(timestamp, SYSCALL_EVENT, op, tid, event_core, procname)
{
	ret_time = 0.0;
	syscall_class = sc_class;
	sc_entry = NULL;
	memset(args, -1, sizeof(uint64_t) * MAX_ARGC);
	nargs = 0;
	ret = -1;
}

bool SyscallEvent::audit_args_num(int size)
{
	if (!sc_entry) {
#if DEBUG_SYSCALL_EVT
		cerr << "sc is null at " << fixed << setprecision(1) << get_abstime() << endl;
#endif
		return false;
	}
	if (nargs + size > MAX_ARGC) { //|| !sc_entry || !sc_entry->args[nargs])
#if DEBUG_SYSCALL_EVT
		cerr << "args number " << nargs + size << " exceeds " << MAX_ARGC\
			<< " at " << fixed << setprecision(1) << get_abstime() << endl;
#endif
		return false;
	}
	if (!sc_entry->args[nargs]) {
#if DEBUG_SYSCALL_EVT
		cerr << "args index " << nargs << " is invalid at "\
			<< fixed << setprecision(1) << get_abstime() << endl;
		cerr << "Prev syscall doesn't returned. Most possible a new syscall event begins." << endl;
#endif
		return false;
	}
	return true;
}

bool SyscallEvent::set_args(uint64_t *arg_array, int size)
{
	if (audit_args_num(1) == false) {
#if DEBUG_SYSCALL_EVT
		cerr << "Argument audit failed at "\
			<< fixed << setprecision(1) << get_abstime() << endl;
#endif
		return false;
	}
	copy(arg_array, arg_array + size, args + nargs);
	nargs += size;
	return true;
}

uint64_t SyscallEvent::get_arg(int idx)
{
	if (idx < nargs && sc_entry->args[idx] != NULL)
		return args[idx];
#if DEBUG_SYSCALL_EVT
	cerr << "get arg fails" << endl;
#endif
	assert(idx < nargs && sc_entry->args[idx] != NULL);
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
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\t" << class_name();
	if (ret_time > 0)
		outfile << "\n\treturn = " << hex << ret << endl;
	if (!sc_entry)
		return;

	outfile << "\n\t" << sc_entry->syscall_name;
	for (int i = 0; i < MAX_ARGC; i++)
		if (sc_entry->args[i])
			outfile << "\n\t" << sc_entry->args[i] << " = " << args[i];
}

void SyscallEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	if (ret_time > 0) {
		outfile << "\t" << fixed << setprecision(1) << ret_time;
		outfile << "\tret = " << hex << ret;
	}
	if (!sc_entry) {
		outfile << endl;
		return;
	}

	for (int i = 0; i < MAX_ARGC; i++)
		if (sc_entry->args[i] != NULL)
			outfile << ",\t" << sc_entry->args[i] << " = " << args[i];

	outfile << endl;
}
