#include "workq_next.hpp"
WqnextEvent::WqnextEvent(double timestamp, string op, uint64_t tid, uint64_t wq, uint64_t thread, uint64_t idl_n, uint64_t req_n, uint32_t coreid, string procname)
:EventBase(timestamp, WQNEXT_EVENT, op, tid, coreid, procname)
{
	workq = wq;
	thr = thread;
	thidlecount = idl_n;
	reqcount = req_n;
	nextthr = (uint64_t) -1;
	is_reuse = false;
	block_type = (uint32_t) NON_BLOCK;
} 

void WqnextEvent::set_block_type(bool timer)
{
	if (timer == true)
		block_type = TIMER_BLOCK;
	else
		block_type = NO_TIMER;
}

void WqnextEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\t - " << fixed << setprecision(1) << get_finish_time();
	outfile << "\n\t" << "next_work_thread = " << nextthr << endl;
}

void WqnextEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t - " << fixed << setprecision(1) << get_finish_time();
	outfile << "\t";
	switch(wqnext_type) {
		case 1 : outfile << "_exp_mkrun";
				break;
		case 3 : {
				if (nextthr != 0)
					outfile << "_exp_wait" << "_tid_" << nextthr;
				else
					outfile << "_ret_false";
				break;
			}
		case 4 : outfile << "_exp_reuse";
				break;
		default:
				break;
	}
	outfile << endl;
}
