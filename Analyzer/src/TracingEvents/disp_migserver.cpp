#include "dispatch.hpp"

DispMigEvent::DispMigEvent(double abstime, string op, uint64_t tid, uint32_t core_id, string procname)
:EventBase(abstime, DISP_MIG_EVENT, op, tid, core_id, procname)
{
}
void DispMigEvent::save_owner(void *_owner)
{
	owner = _owner;
}

void *DispMigEvent::restore_owner(void)
{
	return owner;
}

void DispMigEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << endl;
}

void DispMigEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << endl;
}
