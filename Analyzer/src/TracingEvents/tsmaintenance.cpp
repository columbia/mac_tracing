#include "base.hpp"
#include "tsmaintenance.hpp"

TsmaintenanceEvent::TsmaintenanceEvent(double abstime, string op, uint64_t tid, uint32_t event_core, string procname)
:EventBase(abstime, TSM_EVENT, op, tid, event_core, procname)
{
	preempted_group_ptr = NULL;
}

void * TsmaintenanceEvent:: load_gptr(void)
{
	void * ret = preempted_group_ptr;
	preempted_group_ptr = NULL;
	return ret;
}

void TsmaintenanceEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
}

void TsmaintenanceEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << "timeshared_maintenance" << endl;
}
