#include "base.hpp"

EventBase::EventBase(double _timestamp, string _op, uint64_t _tid, uint32_t _core_id, string _procname)
{
	timestamp = _timestamp;
	op = _op;
	tid = _tid;
	pid = -1;
	core_id = _core_id;
	procname = _procname;
	group_id = (uint64_t)-1;
	complete = false;
	ground = false;
	infected = false;
}

EventBase::EventBase(EventBase * base)
{
	*this = *base;
}

void EventBase::decode_event(bool is_verbose, ofstream & outfile) 
{
	outfile << "\nGeneral Event Decode\n";
	outfile << "[" << hex << pid << "]" << procname;
	outfile << "(" << hex << tid << ")";
	outfile << "\t" << op << "\n";
}
