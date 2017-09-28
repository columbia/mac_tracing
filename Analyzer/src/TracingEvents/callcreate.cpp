#include "timer_callout.hpp"

TimerCreateEvent::TimerCreateEvent(double timestamp, string op, uint64_t _tid, uint32_t event_core, 
		void * funcptr, uint64_t p0, uint64_t p1, void * qptr, string procname)
:EventBase(timestamp, TMCALL_CREATE_EVENT, op, _tid, event_core, procname)
{
	func_ptr = funcptr;
	param0 = p0;
	param1 = p1;
	q_ptr = qptr;
	is_called = false;
	is_cancelled = false;
	timercallout_event = NULL;
	cancel_event = NULL;
}

void TimerCreateEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tqueue " << hex << q_ptr << endl;

	if (is_called) {
		assert(timercallout_event);
		outfile << "\n\t" << "called at " << fixed << setprecision(1) << timercallout_event->get_abstime();
	} else
		outfile << "\n\t" << "not called";

	outfile << "\n\tfunc_" << hex << func_ptr;
	outfile << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
}

void TimerCreateEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tarm_timercallout_func_" << hex << func_ptr;
	outfile << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")";

	if (is_called == true) {
		assert(timercallout_event != NULL);
		outfile << "\n\t" << "called at " << fixed << setprecision(1) << timercallout_event->get_abstime();
		outfile << "\t" << hex << timercallout_event->get_tid();
		outfile << "\t" << timercallout_event->get_procname();
	}
	outfile << endl;
}
