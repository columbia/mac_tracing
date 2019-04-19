#include "parser.hpp"
#include "breakpoint_trap.hpp"

namespace Parse
{
	BreakpointTrapParser::BreakpointTrapParser(string filename)
	:Parser(filename)
	{}

	void BreakpointTrapParser::symbolize_addr(BacktraceParser *backtrace_parser, images_t *image,  breakpoint_trap_ev_t *breakpoint_trap_event, debug_data_t *cur_debugger)
	{
		frame_info_t frame_info;
		vector<uint64_t> addrs = breakpoint_trap_event->get_addrs();

		for (int i = 0; i < addrs.size(); i++) {
			if (addrs[i] == 0)
				continue;
			frame_info.addr = addrs[i];
			frame_info.symbol = "";
			if (frame_info.filepath = image->search_path(frame_info.addr), frame_info.filepath.size() > 0) {
				Frames::checking_symbol_with_image_in_memory(frame_info.symbol, frame_info.addr,
						frame_info.filepath, backtrace_parser->get_vmsymbol_map(), image);
			}
			breakpoint_trap_event->update_target(i, frame_info.symbol);
		}
	}

	void BreakpointTrapParser::symbolize_eip(BacktraceParser *backtrace_parser, images_t *image, breakpoint_trap_ev_t *breakpoint_trap_event, debug_data_t *cur_debugger)
	{
		bool ret;
		frame_info_t frame_info = {.addr = breakpoint_trap_event->get_eip()};
		ret = Frames::lookup_symbol_via_lldb(cur_debugger, &frame_info);
		if (ret == false)
			return;
		if (frame_info.filepath.find("CoreGraphics") != string::npos)
			Frames::checking_symbol_with_image_in_memory(frame_info.symbol, frame_info.addr,
					frame_info.filepath, backtrace_parser->get_vmsymbol_map(), image);
		//string rip_symbol = frame_info.symbol;// + "\t" + frame_info.filepath;
		breakpoint_trap_event->set_caller_info(frame_info.symbol);
	}

	void BreakpointTrapParser::symbolize_hwbrtrap_for_proc(BacktraceParser *backtrace_parser, string procname)
	{
		if (backtrace_parser == NULL)
			return;

		images_t *image = backtrace_parser->get_image_for_proc(-1, procname);
		if (image == NULL) {
			cerr << "warning: lldb fail to get image for " << procname << endl;
			return;
		}

		list<event_t *>::iterator it;

		debug_data_t cur_debugger;
		bool ret = backtrace_parser->setup_lldb(&cur_debugger, image);
		int event_count = 0;
		if (ret == false)
			goto clear_debugger;

		cerr << "load lldb for symbolize hwbr caller... in " << procname << endl;
		for (it = local_event_list.begin(); it != local_event_list.end(); it++) {
			breakpoint_trap_ev_t *breakpoint_trap_event = dynamic_cast<breakpoint_trap_ev_t *>(*it);
			if (breakpoint_trap_event->get_procname() != procname) {
				continue;
			}
			event_count++;
			symbolize_eip(backtrace_parser, image, breakpoint_trap_event, &cur_debugger);
			symbolize_addr(backtrace_parser, image, breakpoint_trap_event, &cur_debugger);
		}
		cerr << "size of hwbr event list for " << procname << " = " << event_count << endl; 
		
	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
	}

	void BreakpointTrapParser::symbolize_hwbrtrap(BacktraceParser *backtrace_parser)
	{
		ifstream input(LoadData::meta_data.libs, ifstream::in);
		if (!input.is_open()) {
			cerr << "fail to open file " << LoadData::meta_data.libs << endl;
			exit(1);
		}
		if (!input.good()) {
			input.close();
			cerr << LoadData::meta_data.libs << " file is broken" << endl;
			exit(1);
		}
		string proc_name;
		while(getline(input, proc_name)) {
			symbolize_hwbrtrap_for_proc(backtrace_parser, proc_name);
		}
		input.close();
		//symbolize_hwbrtrap_for_proc(backtrace_parser, "WindowServer");
		//symbolize_hwbrtrap_for_proc(backtrace_parser, LoadData::meta_data.host);
	}

	void BreakpointTrapParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t eip, arg1, arg2, arg3, tid, coreid;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> eip >> arg1 >> arg2 >> arg3) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}

			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 

			if (breakpoint_trap_events.find(tid) == breakpoint_trap_events.end()) {
				breakpoint_trap_ev_t *breakpoint_trap_event = new breakpoint_trap_ev_t(abstime, opname, tid, eip, coreid, procname);
				if (!breakpoint_trap_event) {
					cerr << "OOM " << __func__ << endl;
					exit(EXIT_FAILURE);
				}
				breakpoint_trap_events[tid] = breakpoint_trap_event;
				breakpoint_trap_event->add_value(arg1 >> 32);
				breakpoint_trap_event->add_value((uint32_t)arg1);
				breakpoint_trap_event->add_value(arg2 >> 32);
				breakpoint_trap_event->add_value((uint32_t)arg2);
				if (arg3)
					breakpoint_trap_event->add_addr(arg3);
				local_event_list.push_back(breakpoint_trap_event);
			} else {
				breakpoint_trap_ev_t *breakpoint_trap_event = breakpoint_trap_events[tid];
				if (arg1)
					breakpoint_trap_event->add_addr(arg1);
				if (arg2)
					breakpoint_trap_event->add_addr(arg2);
				if (arg3)
					breakpoint_trap_event->add_addr(arg3);
				
				breakpoint_trap_events.erase(tid);
				breakpoint_trap_event->set_complete();
			}
		}
	} 

}
