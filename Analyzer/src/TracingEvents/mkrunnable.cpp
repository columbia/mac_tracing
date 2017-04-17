#include "mkrun.hpp"
#include "wait.hpp"

MkrunEvent::MkrunEvent(double timestamp, string op, uint64_t _tid, uint64_t _peer_tid, uint64_t wakeup_event, uint64_t _mr_type, pid_t _pid, pid_t _peer_pid, uint32_t _coreid, string procname)
	:EventBase(timestamp, op, _tid, _coreid, procname)
{
	peer_tid = _peer_tid;
	peer_wakeup_event = wakeup_event;
	mr_type = _mr_type;
	set_pid(_pid);
	peer_pid = _peer_pid;
	wait = NULL;
	peer_event = NULL;
}

void MkrunEvent::pair_wait(wait_ev_t* _wait)
{
	wait = _wait;
}

void MkrunEvent::decode_event(bool is_verbost, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n ["  << dec << get_pid() << "] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\t\t" << get_op() << endl;
	outfile << "\t\t peer_tid " << peer_tid;
	outfile << "\tevent_addr " << peer_wakeup_event;
	if (wait != NULL) {
		outfile << "\n\t" << fixed << setprecision(2) << "wait_at " << wait->get_abstime();
		outfile << "\n\t" << fixed << setprecision(2) << "(period = ";
		outfile << get_abstime() - wait->get_abstime() << ")";
		//wait->decode_event(false, outfile);
	} else {
		outfile << "\n\t[missing wait event]" << endl;
	}
	outfile << endl;
}

void MkrunEvent::streamout_event(ofstream & outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << hex << get_tid() << "\t" << get_procname();
	if (mr_type == WORKQ_MR) {
		outfile << "\twakeup_workthread_" << peer_tid << endl;
	} else {
		outfile << "\twakeup_";
		if (wait != NULL) {
			//outfile << wait->get_wait_event();
			outfile << wait->get_wait_resource();
		} else {
			outfile << "x";
		}
		outfile << "\t" << hex << peer_tid << endl;
	}
}
