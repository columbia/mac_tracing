#include "wait.hpp"

WaitEvent::WaitEvent(double timestamp, string op, uint64_t tid, uint64_t _wait_event, uint32_t coreid, string procname)
:EventBase(timestamp, WAIT_EVENT, op, tid, coreid, procname)
{
	wait_event = _wait_event;
	mkrun_event = NULL;
	syscall_event = NULL;
}

double WaitEvent::get_time_cost(void)
{
	if (mkrun_event != NULL)
		return mkrun_event->get_abstime() - get_abstime();
	if (syscall_event != NULL)
		return syscall_event->get_ret_time() - get_abstime();
	return 0.0;
}

const char * WaitEvent::decode_wait_result(void) 
{
	switch(wait_result) {
		case THREAD_WAITING:
			return "waiting";
		case THREAD_AWAKENED:
			return "awakened";
		case THREAD_TIMED_OUT:
			return "timed_out";
		case THREAD_INTERRUPTED:
			return "interrupted";
		case THREAD_RESTART:
			return "restart";
		case THREAD_NOT_WAITING:
			return "not_waiting";
		default:
			return "unknown";
	}
}

void WaitEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	const char *wait_result_name = decode_wait_result();
	outfile << "\n\t" << hex << wait_event;
	outfile << "\n\t" << wait_result_name;

	if (wait_resource.size())
		outfile << "\n\t" << wait_resource;
	outfile << endl;
}

void WaitEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	const char *wait_result_name = decode_wait_result();
	outfile << "\twait_" << wait_resource;
	outfile << "\t" << wait_result_name;
	if (syscall_event) {
		outfile << "\twait from " << fixed << setprecision(1) << syscall_event->get_abstime();

//	if (syscall_event->get_ret_time() > 0)
//	outfile << "\tsyscall cost " << fixed << setprecision(1) << syscall_event->get_ret_time() - syscall_event->get_abstime() << endl;
	}
	outfile << "\ttime cost " << fixed << setprecision(1) << get_time_cost() << endl;
	outfile << endl;
}
