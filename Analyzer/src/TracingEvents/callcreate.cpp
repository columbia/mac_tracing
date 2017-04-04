#include "timer_callout.hpp"
TimerCreateEvent::TimerCreateEvent(double timestamp, string op, uint64_t _tid, uint32_t event_core, 
				void * funcptr, uint64_t p0, uint64_t p1, void * qptr, string procname)
	: EventBase(timestamp, op, _tid, event_core, procname)
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
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "\t" << hex << q_ptr << endl;
	if (is_called) {
		outfile << "\n\t" << "called" << endl;
		assert(timercallout_event);
		outfile << "\n\t" << fixed << setprecision(2) << timercallout_event->get_abstime();
	} else
		outfile << "\n\t" << "not called" << endl;
	outfile << "\n\tfunc_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")" << endl;
}

void TimerCreateEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\tarm_timercallout_";
	outfile << "func_" << hex << func_ptr << "(" << hex << param0 << ",";
	outfile << hex << param1 << ")";
	if (is_called == true) {
		assert(timercallout_event != NULL);
		outfile << "\t" << hex << timercallout_event->get_tid() << "\t" << timercallout_event->get_procname();
	}
	outfile << endl;
}
