#include "base.hpp"
#include "tsmaintenance.hpp"

TsmaintenanceEvent::TsmaintenanceEvent(double abstime, string op, uint64_t tid, uint32_t event_core, string procname)
:EventBase(abstime, op, tid, event_core, procname)
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
	
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "\t";
	outfile << endl;
}

void TsmaintenanceEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();//<< tid_comm[get_tid()];
	outfile << "\t" << "timeshared_maintenance" << endl;
}
