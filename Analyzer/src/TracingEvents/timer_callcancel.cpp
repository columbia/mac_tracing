#include "timer_call.hpp"

TimerCancelEvent::TimerCancelEvent(double timestamp, string op, uint64_t _tid, uint32_t event_core,
						void * funcptr, uint64_t p0, uint64_t p1, void * qptr, string procname)
:EventBase(timestamp, TMCALL_CANCEL_EVENT, op, _tid, event_core, procname)
{
	create_event = NULL;
	func_ptr = funcptr;
	param0 = p0;
	param1 = p1;
	q_ptr = qptr;
}

bool TimerCancelEvent::check_root(timercreate_ev_t * event)
{
	if (func_ptr != event->get_func_ptr()
		|| param0 != event->get_param0()
		|| param1 != event->get_param1())
		//|| q_ptr != event->get_q_ptr())
		return false;

	if (event->check_cancel() == true) {
		cerr << "Error:\n";
		cerr << "timercallout cancel at " << fixed << setprecision(2) << get_abstime() << endl;
		cerr << "try to cancel timer created at" << fixed << setprecision(2) << event->get_abstime() << endl;
		cerr << "\t cancelled at " << fixed << setprecision(2) << event->get_cancel_peer()->get_abstime() << endl;
	}
	return true;
}

void TimerCancelEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tqueue " << hex << q_ptr << endl;
	outfile << "\n\tfunc_" << hex << func_ptr;
	outfile << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;

	if (create_event) {
		outfile << "\n\tcreator: ";
		outfile << fixed << setprecision(1) << create_event->get_abstime();
	}
}

void TimerCancelEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tkern_timercancel_func_" << hex << func_ptr;
	outfile << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
}
