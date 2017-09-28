#include "cadisplay.hpp"
CADispEvent::CADispEvent(double abstime, string _op, uint64_t _tid, uint64_t object, uint32_t _core_id, string _procname)
:EventBase(abstime, CA_DISPLAY_EVENT, _op, _tid, _core_id, _procname)
{
	object_addr = object;
	object_set_events.clear();
	is_serial = 0;
}

void CADispEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\t" << hex << object_addr;
}

void CADispEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << hex << object_addr;
	outfile << "\t" << object_set_events.size() << endl;
}
