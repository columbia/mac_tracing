#include "mkrun.hpp"
FakedwokenEvent::FakedwokenEvent(double timestamp, string op, uint64_t tid, mkrun_ev_t *_mkrun_peer, uint32_t coreid, string procname)
:EventBase(timestamp, FAKED_WOKEN_EVENT, op, tid, coreid, procname)
{
	mkrun_peer = _mkrun_peer;
}

void FakedwokenEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << endl;
}

void FakedwokenEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\twaker " << hex << mkrun_peer->get_tid() << "\t" << fixed << setprecision(1) << mkrun_peer->get_abstime();
	outfile << endl;
}
