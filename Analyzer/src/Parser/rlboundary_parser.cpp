#include "parser.hpp"
#include "rlboundary.hpp"

namespace Parse
{
	RLBoundaryParser::RLBoundaryParser(string filename)
	:Parser(filename)
	{}
	void RLBoundaryParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t state, func_ptr, block, unused, tid, coreid;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> state >> func_ptr >> block >> unused) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}

			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 
			
			rl_boundary_ev_t *rl_boundary_event = NULL;
			if (state == SetSignalForSource0 || state == UnsetSignalForSource0) {
				rl_boundary_event = new rl_boundary_ev_t(abstime, opname, tid, state, 0, coreid, procname);
				rl_boundary_event->set_rls(func_ptr);
			} 
			else {
				rl_boundary_event = new rl_boundary_ev_t(abstime, opname, tid, state, func_ptr, coreid, procname);
				//if (opname =="RL_DoBlocks" && (state == ItemEnd || state == BlockPerform))
				if (opname =="RL_DoBlocks")
					rl_boundary_event->set_block(block);
			}

			if (!rl_boundary_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}
			rl_boundary_event->set_complete();
			local_event_list.push_back(rl_boundary_event);
		}
	}

	void RLBoundaryParser::symbolize_rlboundary_callbacks(BacktraceParser *parser)
	{
		if (!parser)
			return;
		images_t *host_image = parser->get_host_image();
		if (!host_image)
			return;

		debug_data_t cur_debugger;

		cerr << "load lldb for rlboundary events ..." << endl;

		if (!parser->setup_lldb(&cur_debugger, host_image))
			goto clear_debugger;
		symbolize_func_ptr(&cur_debugger);

		cerr << "finish dispatch event symbolization." << endl;

	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
	}

	void RLBoundaryParser::symbolize_func_ptr(debug_data_t *cur_debugger_ptr)
	{
		list<event_t *>::iterator it;
		rl_boundary_ev_t *rl_boundary_event;
		string desc;
		bool ret;

		for (it = local_event_list.begin(); it != local_event_list.end();
			it++) {
			rl_boundary_event = dynamic_cast<rl_boundary_ev_t *>(*it);
			if (!rl_boundary_event || rl_boundary_event->get_procname() != LoadData::meta_data.host)
				continue;

			desc = "";
			frame_info_t frame_info = {.addr = rl_boundary_event->get_func_ptr()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true) {
				desc.append(frame_info.symbol);
				desc.append(frame_info.filepath);
			}
			rl_boundary_event->set_func_symbol(desc);
		}
	}
}
