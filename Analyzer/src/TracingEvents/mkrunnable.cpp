#include "mkrun.hpp"
#include "wait.hpp"

MkrunEvent::MkrunEvent(double timestamp, string op, uint64_t _tid, uint64_t _peer_tid, uint64_t wakeup_event, uint64_t _mr_type, pid_t _pid, pid_t _peer_pid, uint32_t _coreid, string procname)
:EventBase(timestamp, MR_EVENT, op, _tid, _coreid, procname)
{
	peer_tid = _peer_tid;
	peer_wakeup_event = wakeup_event;
	mr_type = _mr_type;
	set_pid(_pid);
	peer_pid = _peer_pid;
	wait = NULL;
	peer_event = NULL;
}

bool MkrunEvent::check_interrupt(intr_ev_t *potential_root)
{
	double begin = potential_root->get_abstime();
	double end = potential_root->get_finish_time();
	double mr_time = get_abstime();
	if (mr_time - begin >= 10e-8 && end - mr_time >= 10e-8)
		return true;
	return false;
}

int32_t MkrunEvent::check_mr_type(event_t *last_event, intr_ev_t *potential_root)
{
	if (!last_event)
		return mr_type;

	int last_event_type = last_event->get_event_id();
	double mr_time = get_abstime();
	double begin = last_event->get_abstime(), end;

	switch (last_event_type) {
		case TSM_EVENT:
			mr_type = SCHED_TSM_MR;
			break;
		case TMCALL_CALLOUT_EVENT:
			mr_type = CALLOUT_MR;
			break;
		case WQNEXT_EVENT:
			end = last_event->get_finish_time();
			if (mr_time - begin >= 10e-8 && end - mr_time >= 10e-8)
				mr_type = WORKQ_MR;
			break;
		default:
			if (!potential_root)
				break;
			if (check_interrupt(potential_root) == true) {
				potential_root->add_invoke_thread(peer_tid);
				mr_type = INTR_MR;
			}
			break;
	}
	return mr_type;
}

void MkrunEvent::pair_wait(wait_ev_t* _wait)
{
	wait = _wait;
	set_event_peer(wait);
}

void MkrunEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\t peer_tid " << peer_tid;
	outfile << "\n\tevent_addr " << peer_wakeup_event;

	if (wait) {
		outfile << "\n\t" << fixed << setprecision(1) << "wait_at " << wait->get_abstime();
		outfile << "\n\t" << fixed << setprecision(1) << "(period = ";
		outfile << get_abstime() - wait->get_abstime() << ")";
	} else {
		outfile << "\n\t[missing wait event]" << endl;
	}
	outfile << "\n\t" << mr_type << endl;
	outfile << endl;
}

void MkrunEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);

	if (mr_type == WORKQ_MR) {
		outfile << "\twakeup_workthread_" << peer_tid << endl;
	} else {
		outfile << "\twakeup_";
		if (wait)
			outfile << wait->get_wait_resource();
		else
			outfile << "x";
		outfile << "\t" << hex << peer_tid;
	}
	outfile << "\tmr_type " << mr_type << endl;
}
