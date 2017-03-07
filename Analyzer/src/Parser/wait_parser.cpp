#include "parser.hpp"
#include "wait.hpp"

namespace Parse
{
	WaitParser::WaitParser(string filename)
	:Parser(filename)
	{
		wait_events.clear();
	}
	
	bool WaitParser::set_info(wait_ev_t * wait_event, uint64_t pid, uint64_t deadline, uint64_t wait_result)
	{
		wait_event->set_pid(lo(pid));
		wait_event->set_wait_result(wait_result);
		return true;
	}

	bool WaitParser::new_wait_event(string opname, double abstime, istringstream &iss)
	{
		uint64_t disc[3] = {0};
		uint64_t wait_event, tid, coreid;
		string procname;
		if (!(iss >> hex >> wait_event >> disc[0] >> disc[1] >> disc[2] >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = ""; 
		assert(wait_events.find(tid) == wait_events.end());
		wait_ev_t * new_evw = new wait_ev_t(abstime, opname, tid, wait_event, (uint32_t)coreid, procname);
		{
			char s[32] = "";
			int idx = 0;
			LittleEndDecoder::decode64(disc[0], s, &idx); 
			LittleEndDecoder::decode64(disc[1], s, &idx);
			LittleEndDecoder::decode64(disc[2], s, &idx);
			new_evw->set_wait_resource(s);
		}
		local_event_list.push_back(new_evw);
		wait_events[tid] = new_evw;
		return true;
	}	
	
	bool WaitParser::finish_wait_event(string opname, double abstime, istringstream &iss)
	{
		uint64_t wait_event, pid, deadline, wait_result, tid, coreid;
		if (!(iss >> hex >> wait_event >> pid >> deadline >> wait_result >> tid >> coreid))
			return false;
	
		if (wait_events.find(tid) == wait_events.end()) {
			outfile << "Error : missing wait reason for wait event at " << fixed << setprecision(1) << abstime << endl;
			string procname;
			if (!getline(iss >> ws, procname) || !procname.size())
				procname = "";
			wait_ev_t *wait_evt = new wait_ev_t(abstime, opname, tid, wait_event, (uint32_t)coreid, procname);
			set_info(wait_evt, pid, deadline, wait_result);
			wait_evt->set_complete();
			local_event_list.push_back(wait_evt);
			return false;
		}
		//assert(wait_events.find(tid) != wait_events.end());
		wait_ev_t * wait_evt = wait_events[tid];
		assert(wait_event == wait_evt->get_wait_event());
		wait_evt->override_timestamp(abstime);
		set_info(wait_evt, pid, deadline, wait_result);
		wait_events.erase(tid);
		wait_evt->set_complete();
		return true;
	}
	
	void WaitParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret = false;
		while(getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}
			ret = false;
			if (opname == "MACH_WAIT_REASON") {
				ret = new_wait_event(opname, abstime, iss);
			} else if (opname == "MACH_WAIT") {
				ret = finish_wait_event(opname, abstime, iss);
			} else {
				cerr << "Error: unkown opname" << endl;
			}
			if (ret == false)
				outfile << line << endl;
		}
		// check wait_events
	}
}
