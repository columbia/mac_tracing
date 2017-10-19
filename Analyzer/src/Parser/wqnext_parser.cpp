#include "parser.hpp"
#include "workq_next.hpp"

namespace Parse
{
	WqnextParser::WqnextParser(string filename)
	:Parser(filename)
	{
		wqnext_events.clear();
	}

	bool WqnextParser::set_info(wqnext_ev_t *wqnext_event, uint64_t wqnext_type,
		 uint64_t next_thread, uint64_t arg3)
	{
		/* if arg4 == 1 workq run req with a new thread
		 * if arg4 == 3 start a timer(check arg3) or block
		 *  arg2 = threadid or zero
		 * if arg4 == 4 wq, arg2 = arg3 = 0, thread reused
		 */
		wqnext_event->set_wqnext_type(wqnext_type);
		switch (wqnext_type) {
			case 1:
				wqnext_event->set_run_mode(arg3);
				wqnext_event->set_nextthr(next_thread);
				break;
			case 3:
				wqnext_event->set_block_type((bool)arg3);
				wqnext_event->set_nextthr(next_thread);
				break;
			case 4:
				assert(next_thread == 0 && arg3 == 0);
				wqnext_event->set_reuse();
				break;
			default:
				return false;
		}
		wqnext_event->set_complete();
		return true;
	}

	bool WqnextParser::finish_wqnext_event(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t wq, next_thread, arg3, wqnext_type, tid, coreid; 
		string procname;
		if (!(iss >> hex >> wq >> next_thread >> arg3 >> wqnext_type \
			>> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (wqnext_events.find(tid) == wqnext_events.end())
			return false;
	
		wqnext_ev_t *wqnext_event = wqnext_events[tid];
		assert(wq == wqnext_event->get_workq());	
		assert(set_info(wqnext_event, wqnext_type, next_thread, arg3));
		wqnext_event->set_finish_time(abstime);
		wqnext_events.erase(tid);
		return true;
	}

	bool WqnextParser::new_wqnext_event(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t wq, next_thread, idle_count, req_count, tid, coreid;
		string procname;

		if (!(iss >> hex >> wq >> next_thread >> idle_count >> req_count \
			>> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (wqnext_events.find(tid) != wqnext_events.end()) {
			outfile << "Already exist at " << fixed << setprecision(1) << wqnext_events[tid]->get_abstime() << endl;
			return false;
		}

		wqnext_ev_t *new_wqnext = new wqnext_ev_t(abstime, opname, tid,
			wq, next_thread, idle_count, req_count, coreid, procname);

		if (!new_wqnext) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		wqnext_events[tid] = new_wqnext;
		local_event_list.push_back(new_wqnext);
		return true;
	}

	void WqnextParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		bool ret = false;

		while(getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}

			ret = false;
			if (deltatime.find("(") == string::npos)
				ret = new_wqnext_event(opname, abstime, iss);
			else
				ret = finish_wqnext_event(opname, abstime, iss);

			if (ret == false)
				outfile << line << endl;
		}
		// need check wqnext_events
	}
	
}
