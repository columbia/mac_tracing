#include "timer_callout.hpp"
TimerCalloutEvent::TimerCalloutEvent(double timestamp, string op, uint64_t _tid, uint32_t event_core, 
	void * funcptr, uint64_t p0, uint64_t p1, void * qptr, string procname)
:EventBase(timestamp, TMCALL_CALLOUT_EVENT, op, _tid, event_core, procname)
{
	create_event = NULL;
	func_ptr = funcptr;
	param0 = p0;
	param1 = p1;
	q_ptr = qptr;
}

bool TimerCalloutEvent::check_root(timercreate_ev_t * event)
{
	if (func_ptr != event->get_func_ptr()
		|| param0 != event->get_param0())
		//|| param1 != event->get_param1()
		//|| q_ptr != event->get_q_ptr())
		return false;
	

	if (event->check_called() == true) {
		cerr << "Error:\n";
		cerr << "timercallout at " << fixed << setprecision(2) << get_abstime() << endl;
		cerr << "try to match called create " << fixed << setprecision(2) << event->get_abstime() << endl;
		cerr << "calleded at " << fixed << setprecision(2) << event->get_called_peer()->get_abstime() << endl;
	}
	
	if (event->check_cancel() == true) {
		cerr << "Error:\n";
		cerr << "timercallout at " << fixed << setprecision(2) << get_abstime() << endl;
		cerr << "try to match cancelled create " << fixed << setprecision(2) << event->get_abstime() << endl;
		cerr << "canceled at " << fixed << setprecision(2) << event->get_cancel_peer()->get_abstime() << endl;
	}
	return true;
}

void TimerCalloutEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tqueue" << hex << q_ptr << endl;
	outfile << "\n\tfunc_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
	if (create_event != NULL) {
		outfile << "\n\tget creator: ";
		outfile << "\n\t" << fixed << setprecision(2) << create_event->get_abstime();
	}
}

void TimerCalloutEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tkern_timercallout_";
	outfile << "func_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")";
	if (create_event != NULL) {
		outfile << "\n\ttimer armed at: ";
		outfile << "\t" << fixed << setprecision(2) << create_event->get_abstime();
	}
	outfile << endl;
}
