#include "base.hpp"

EventBase::EventBase(double _timestamp, int _event_id, string _op, uint64_t _tid, uint32_t _core_id, string _procname)
{
	timestamp = _timestamp;
	event_id = _event_id;
	op = _op;
	tid = _tid;
	pid = -1;
	core_id = _core_id;
	procname = _procname;
	group_id = (uint64_t)-1;
	complete = false;
}

EventBase::EventBase(EventBase * base)
{
	*this = *base;
}

void EventBase::decode_event(bool is_verbose, ofstream &outfile) 
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << get_op();
	outfile << "\n\t" << fixed << setprecision(1) << get_abstime();
	outfile << endl;
}

void EventBase::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id();
	outfile << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid();
	outfile << "\t" << get_procname();
	outfile << "\t" << get_op();
}
