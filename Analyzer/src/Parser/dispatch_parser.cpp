#include "parser.hpp"
#include "dispatch.hpp"

namespace Parse
{
	DispatchParser::DispatchParser(string filename)
	:Parser(filename)
	{
		dequeue_events.clear();
	}

	bool DispatchParser::process_enqueue(string opname, double abstime, istringstream &iss)
	{
		uint64_t q_id, item, ref, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> q_id >> item >> ref >> unused >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		enqueue_ev_t *new_enqueue = new enqueue_ev_t(abstime, opname, tid, q_id, item, ref, coreid, procname);
			
		new_enqueue->set_complete();
		local_event_list.push_back(new_enqueue);
		return true;
	}
	bool DispatchParser::is_duplicate_deq(dequeue_ev_t * prev, dequeue_ev_t * cur)
	{
		if (prev->get_ref() != 3)
			return false;
		if (prev->get_qid() != cur->get_qid() 
			|| prev->get_item() != cur->get_item()
			|| prev->get_func_ptr() != cur->get_func_ptr()
			||prev->get_invoke_ptr() != cur->get_invoke_ptr())
			return false;
		return true;
	}

	bool DispatchParser::set_info_for_dequeue(double abstime, uint32_t ref, istringstream &iss)
	{
		uint64_t func_ptr, invoke_ptr, vtable_ptr, tid, corid;
		if (!(iss >> hex >> func_ptr >> invoke_ptr >> vtable_ptr >> tid >> corid))
			return false;

		if (dequeue_events.find(tid) == dequeue_events.end()) {
			cerr << "Check : missing info " << fixed << setprecision(1) << abstime << endl;
			return false;
		}

		dequeue_ev_t * dequeue_event = dequeue_events[tid];
		assert(dequeue_event->get_ref() == ref);
		dequeue_event->set_ptrs(func_ptr, invoke_ptr, vtable_ptr);
		dequeue_event->set_complete();
		dequeue_events.erase(tid);

		if (ref == 4) {
			list<event_t *>::reverse_iterator rit = local_event_list.rbegin();
			dequeue_ev_t * prev;

			for (rit++; rit != local_event_list.rend(); rit++) {
				prev = dynamic_cast<dequeue_ev_t *> (*rit);

				if (prev->get_tid() != tid)
					continue;

				if (dequeue_event->get_abstime() - prev->get_abstime() > 50)
					break;

				if (is_duplicate_deq(prev, dequeue_event)) {
					dequeue_event->set_duplicate();
					local_event_list.erase(next(rit).base());
				}

				break;
			}
		}
		
		return true;
	}
	
	bool DispatchParser::new_dequeue_event(uint32_t ref, string opname, double abstime, istringstream &iss)
	{
		uint64_t q_id, item, ctxt, tid, coreid;
		string procname;
		if (!(iss >> hex >> q_id >> item >> ctxt >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		assert(dequeue_events.find(tid) == dequeue_events.end());
		dequeue_ev_t * new_dequeue = new dequeue_ev_t(abstime, opname, tid,
			q_id, item, ctxt, ref, coreid, procname);
		if (!new_dequeue)
			return false;
		local_event_list.push_back(new_dequeue);
		dequeue_events[tid] = new_dequeue;
		return true;
	}

	bool DispatchParser::process_dequeue(string opname, double abstime, istringstream &iss)
	{
		uint64_t ref;
		if (!(iss >> hex >> ref)) // || Parse::lo(ref) != 3) //pass it
			return false;
		if (Parse::hi(ref) == 0) {
			return new_dequeue_event(Parse::lo(ref), opname, abstime, iss);
		} else {
			assert(Parse::hi(ref) == 1);
			return set_info_for_dequeue(abstime, Parse::lo(ref), iss);
		}
	}

	bool DispatchParser::checking_blockinvoke_pair(blockinvoke_ev_t *new_invoke) 
	{
		if (new_invoke->is_begin()) {
			dispatch_blockinvoke_begin_list.push_back(new_invoke);
			return true;
		} else {
			list<blockinvoke_ev_t *>::reverse_iterator rit;
			for (rit = dispatch_blockinvoke_begin_list.rbegin(); 
				rit != dispatch_blockinvoke_begin_list.rend(); rit++) {
				blockinvoke_ev_t * invokebegin = *rit;
				if (invokebegin->get_tid() == new_invoke->get_tid()
					 && invokebegin->get_func() == new_invoke->get_func()) {
					new_invoke->set_root(invokebegin);
					return true;
				}
			}
			return false;
		}
	}
	
	void DispatchParser::symbolize_ctxt_and_func(BacktraceParser *backtrace_parser)
	{
		if (backtrace_parser == NULL)
			return;

		images_t * host_image =  backtrace_parser->get_host_image();
		if (host_image == NULL)
			return;

		//TODO : we do not need to set up debugger every time
		//lldb::SBDebugger::Initialize();
		debug_data_t cur_debugger;
		bool ret = backtrace_parser->setup_lldb(&cur_debugger, host_image);

		cerr << "load lldb for symbolize block invoke func and ctxt " << endl;
		list<event_t *>::iterator it;
		blockinvoke_ev_t * blockinvoke_event;

		if (ret == false)
			goto clear_debugger;

		for (it = local_event_list.begin(); it != local_event_list.end(); it++) {
			blockinvoke_event = dynamic_cast<blockinvoke_ev_t *>(*it);
			if (!blockinvoke_event
				|| blockinvoke_event->get_procname().find(LoadData::meta_data.host) == string::npos)
				continue;

			string desc = "func_";
			frame_info_t frame_info = {.addr = blockinvoke_event->get_func()};
			ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
			if (ret == true) {
				desc = frame_info.symbol;
			} else {
				desc = "unknown_fun";
			}
			
			frame_info = {.addr = blockinvoke_event->get_ctxt()};
			ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
			if (ret == true) {
				desc.append("_ctxt_");
				desc.append(frame_info.symbol);
			}
			blockinvoke_event->set_desc(desc);
		}
		cerr << "finished dispatch event symbolization" << endl;
		
	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
		//lldb::SBDebugger::Terminate();
	}

	void DispatchParser::symbolize_invoke_func(BacktraceParser *backtrace_parser)
	{
		if (backtrace_parser == NULL)
			return;

		images_t * host_image =  backtrace_parser->get_host_image();
		if (host_image == NULL)
			return;

		//TODO : we do not need to set up debugger every time
		//lldb::SBDebugger::Initialize();
		debug_data_t cur_debugger;
		bool ret = backtrace_parser->setup_lldb(&cur_debugger, host_image);

		cerr << "load lldb for symbolize block invoke func and ctxt " << endl;
		list<event_t *>::iterator it;
		dequeue_ev_t * dequeue_event;

		if (ret == false)
			goto clear_debugger;

		for (it = local_event_list.begin(); it != local_event_list.end(); it++) {
			dequeue_event = dynamic_cast<dequeue_ev_t *>(*it);
			if (!dequeue_event
				|| dequeue_event->get_procname().find(LoadData::meta_data.host) == string::npos)
				continue;

			string desc("_unknown_func_");

			frame_info_t frame_info = {.addr = dequeue_event->get_func_ptr()};
			ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
			if (ret == true) {
				desc = "_func_";
				desc.append(frame_info.symbol);
			} else {
				frame_info = {.addr = dequeue_event->get_invoke_ptr()};
				ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
				if (ret == true) {
					desc = "_invoke_";
					desc.append(frame_info.symbol);
				}
			}
			
			frame_info = {.addr = dequeue_event->get_ctxt()};
			ret = Frames::lookup_symbol_via_lldb(&cur_debugger, &frame_info);
			if (ret == true) {
				desc.append("_ctxt_");
				desc.append(frame_info.symbol);
			}
			dequeue_event->set_desc(desc);
		}
		cerr << "finished dispatch event symbolization" << endl;
		
	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
		//lldb::SBDebugger::Terminate();
	}

	bool DispatchParser::process_execute(string opname, double abstime, istringstream &iss)
	{
		uint64_t _func, _ctxt, _begin, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> _func >> _ctxt >> _begin >> unused >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		blockinvoke_ev_t * new_invoke = new blockinvoke_ev_t(abstime, opname, tid, _func, _ctxt, _begin, coreid, procname);
		if (checking_blockinvoke_pair(new_invoke) == false) {
			outfile << "Error : a blockinvoke end missing counterpart begin. normal if the invoke begins before tracing" << endl;
			delete (new_invoke);
			return false;
		}
		new_invoke->set_complete();
		local_event_list.push_back(new_invoke);
		return true;
	}
	
	void DispatchParser::process()
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
			switch (LoadData::map_op_code(0, opname)) {
				case DISP_ENQ:
					ret =process_enqueue(opname, abstime, iss);
					break;
				case DISP_DEQ:
					ret = process_dequeue(opname, abstime, iss);
					break;
				case DISP_EXE:
					ret = process_execute(opname, abstime, iss);
					break;
				default:
					ret = false;
					outfile << "unknown operation" << endl;
			}
			if (ret == false) {
				outfile << line << endl;
			}
		}
		// check map
	}
}
