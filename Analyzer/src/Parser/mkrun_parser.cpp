#include "parser.hpp"
#include "mkrun.hpp"

namespace Parse
{
	MkrunParser::MkrunParser(string filename)
	:Parser(filename)
	{
		mkrun_events.clear();
	}

	bool MkrunParser::set_info(mkrun_ev_t *mr_event, uint64_t peer_prio, uint64_t wait_result, uint64_t run_info)
	{
		mr_event->set_peer_prio(peer_prio);
		mr_event->set_peer_wait_result(wait_result);
		mr_event->set_peer_run_count(lo(run_info));
		mr_event->set_peer_ready_for_runq(hi(run_info));
		return true;
	}

	bool MkrunParser::process_finish_wakeup(mkrun_ev_t * mr_event, double abstime, istringstream &iss)
	{
		string procname;
		uint64_t peer_prio, wait_result, run_info, tid, coreid;
		if (!(iss >> hex >> peer_prio >> wait_result >> run_info >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (tid != mr_event->get_tid()) {
			outfile << "Error : try to maching wake up that begins at " << fixed << setprecision(2) << mr_event->get_abstime() << endl;
			outfile << "Error : in different threads" << hex << tid << " " << hex << mr_event->get_tid() << endl;
			return false;
		}
		//assert(tid == mr_event->get_tid());
		set_info(mr_event, peer_prio, wait_result, run_info);
		mr_event->override_timestamp(abstime);
		mr_event->set_complete();
		return true;
	}

	bool MkrunParser::process_new_wakeup(uint64_t peer_tid, double abstime, istringstream &iss)
	{
		string procname;
		uint64_t wake_event, wakeup_type, pid, tid, coreid;
		if (!(iss >> hex >> wake_event >> wakeup_type >> pid >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		mkrun_ev_t *new_mr = new mkrun_ev_t(abstime, "Wakeup", tid, peer_tid,
			wake_event, wakeup_type, lo(pid), hi(pid), coreid, procname);
		if (new_mr == NULL)
			return false;
		mkrun_events[peer_tid] = new_mr;
		local_event_list.push_back(new_mr);
		return true;	
	}

	/*
	 * target thread is locked,
	 * so take taget thread id as key is safe
	 */
	void MkrunParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		uint64_t peer_tid;
		bool ret = false;

		while(getline(infile, line)) {
			istringstream iss(line);
			ret = false;
			if (!(iss >> abstime >> deltatime >> opname >> hex >> peer_tid)) {
				outfile << line << endl;
				continue;
			}
			
			if (opname == "MACH_WAKEUP_REASON") {
				assert(mkrun_events.find(peer_tid) == mkrun_events.end());
				ret = process_new_wakeup(peer_tid, abstime, iss);
			} else {
				assert(opname == "MACH_MKRUNNABLE");
				mkrun_ev_t * mr_event = mkrun_events[peer_tid];
				ret = process_finish_wakeup(mr_event, abstime, iss);
				mkrun_events.erase(peer_tid);
			}

			if (ret == false)
				outfile << line << endl;
		}
		// check mkrun_events
	}
}
