#include "timer_callout.hpp"

CallCancelEvent::CallCancelEvent(double timestamp, string op, uint64_t _tid, uint32_t event_core,
						void * funcptr, uint64_t p0, uint64_t p1, void * qptr, string procname)
: EventBase(timestamp, op, _tid, event_core, procname)
{
	create_event = NULL;
	func_ptr = funcptr;
	param0 = p0;
	param1 = p1;
	q_ptr = qptr;
}

bool CallCancelEvent::check_root(callcreate_ev_t * event)
{
	if (func_ptr != event->get_func_ptr()
		|| param0 != event->get_param0())
		//|| param1 != event->get_param1()
		//|| q_ptr != event->get_q_ptr())
		return false;

	if (event->check_cancel() == true) {
		cerr << "Error:\n";
		cerr << "callout cancel at " << fixed << setprecision(2) << get_abstime() << endl;
		cerr << "try to cancel timer created at" << fixed << setprecision(2) << event->get_abstime() << endl;
		cerr << "\t cancelled at " << fixed << setprecision(2) << event->get_cancel_peer()->get_abstime() << endl;
	}
	return true;
}

void CallCancelEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "\t" << hex << q_ptr << endl;
	outfile << "\n\tfunc_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
	if (create_event != NULL) {
		outfile << "\n\tget creator: ";
		outfile << "\n\t" << fixed << setprecision(2) << create_event->get_abstime();
	}
}

void CallCancelEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\tkern_callcancel_";
	outfile << "func_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
}
