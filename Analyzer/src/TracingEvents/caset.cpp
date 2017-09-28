#include "caset.hpp"
CASetEvent::CASetEvent(double abstime, string _op, uint64_t _tid, uint64_t object, uint32_t _core_id, string _procname)
:EventBase(abstime, CA_SET_EVENT, _op, _tid, _core_id, _procname)
{
	object_addr = object;
	display = NULL;
}

void CASetEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tobject_addr " << hex << object_addr << endl;
	if (display)
		outfile << "\n\tdisplay at " << fixed << setprecision(1) << display->get_abstime() << endl;
}

void CASetEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << hex << object_addr;
	if (display)
		outfile << "\t" << fixed << setprecision(1) << display->get_abstime();
	outfile << endl;
}
