#include "wait.hpp"

WaitEvent::WaitEvent(double timestamp, string op, uint64_t tid, uint64_t _wait_event, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	wait_event = _wait_event;
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
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "\t" << hex <<  wait_event;
	const char * wait_result_name = decode_wait_result();
	outfile << "\n\t" << wait_result_name;

	if (wait_resource.size()!=0)
		outfile << "\n\t" << wait_resource;
	outfile << endl;
}

void WaitEvent::streamout_event(ofstream & outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\twait_" << wait_resource;
	outfile << endl;
}
