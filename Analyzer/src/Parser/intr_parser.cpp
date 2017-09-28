#include "parser.hpp"
#include "interrupt.hpp"

namespace Parse
{
	IntrParser::IntrParser(string filename)
	:Parser(filename)
	{
		intr_events.clear();
	}
	
	void IntrParser::symbolize_intr_rip(BacktraceParser *backtrace_parser)
	{
		list<event_t *>::iterator it;
		intr_ev_t * intr_event;
		if (backtrace_parser == NULL)
			return;

		images_t *host_image =  backtrace_parser->get_host_image();
		if (host_image == NULL)
			return;

		debug_data_t cur_debugger;
		bool ret = backtrace_parser->setup_lldb(&cur_debugger, host_image);
		if (ret == false)
			goto clear_debugger;

		cerr << "load lldb for symbolize rips " << endl;
		for (it = local_event_list.begin(); it != local_event_list.end();
			it++) {
			intr_event = dynamic_cast<intr_ev_t *>(*it);
			if (intr_event == NULL
				|| intr_event->get_procname().\
				find(LoadData::meta_data.host) == string::npos\
				|| intr_event->get_user_mode() == 0)
				continue;
			frame_info_t frame_info = {.addr = intr_event->get_rip()};
			ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
			if (ret == true) {
				string rip_symbol = frame_info.symbol
					+ "_at_" + frame_info.filepath;
				intr_event->set_rip_info(rip_symbol);
			}
		}
		cerr << "finished rip symbolization" << endl;
		
	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
	}

	intr_ev_t *IntrParser::create_intr_event(double abstime, string opname,
		istringstream & iss) 
	{
		string procname;
		uint64_t intr_no, rip, user_mode, itype, tid, coreid;

		if (!(iss >> hex >> intr_no >> rip >> user_mode >> itype) ||
			!(iss >> hex >> tid >> coreid))
			return NULL;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = ""; 

		if (intr_events.find(tid) != intr_events.end())
			return NULL;

		intr_ev_t *new_intr = new intr_ev_t(abstime, opname, tid, intr_no, rip,
			user_mode, coreid, procname);
		if (!new_intr) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		intr_events[tid] = new_intr;
		local_event_list.push_back(new_intr);
		return new_intr;
	}

	bool IntrParser::collect_intr_info(double abstime, istringstream &iss)
	{
		string procname;
		uint64_t intr_no, ast, effective_policy, req_policy, tid, coreid;

		if (!(iss >> hex >> intr_no >> ast >> effective_policy >> req_policy)
			|| !(iss >> hex >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = ""; 

		if (intr_events.find(tid) == intr_events.end())
			return false;
		
		intr_ev_t * intr_event = intr_events[tid];

		if ((uint32_t)intr_no != intr_event->get_interrupt_num())
			return false;

		intr_event->set_sched_priority_post((int16_t)(intr_no >> 32));
		intr_event->set_ast(ast);
		intr_event->set_effective_policy(effective_policy);
		intr_event->set_request_policy(req_policy);
		intr_event->set_finish_time(abstime);
		intr_event->set_complete();
		intr_events.erase(tid);
		return true;
	}
	
	void IntrParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool is_begin;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}

			is_begin = deltatime.find("(") != string::npos ? false : true;
			if (is_begin) {
				if (!create_intr_event(abstime, opname, iss))
					outfile  << line << endl;
			} else {
				if (!collect_intr_info(abstime, iss))
					outfile << line << endl;
			}
		}
		//TODO: need check intr_events;
	}
}
