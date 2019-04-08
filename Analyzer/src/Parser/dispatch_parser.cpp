#include "parser.hpp"
#include "dispatch.hpp"

namespace Parse
{
	DispatchParser::DispatchParser(string filename)
	:Parser(filename)
	{
		dequeue_events.clear();
	}

	bool DispatchParser::process_enqueue(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t q_id, item, ref = 0, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> ref >> q_id >> item >> unused >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (!(ref >> 32)) {
			enqueue_ev_t *new_enqueue = new enqueue_ev_t(abstime, opname, \
				tid, q_id, item, ref, coreid, procname);

			if (!new_enqueue) {
				cerr << "OOM: " << __func__ << endl;
				return false;
			}

			new_enqueue->set_complete();
			local_event_list.push_back(new_enqueue);
			return true;
		}
		return true;
	}

	bool DispatchParser::is_duplicate_deq(dequeue_ev_t *prev, dequeue_ev_t *cur)
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

	bool DispatchParser::set_info_for_dequeue(double abstime, uint32_t ref,
		istringstream &iss)
	{
		uint64_t func_ptr, invoke_ptr, vtable_ptr, tid, corid;
		if (!(iss >> hex >> func_ptr >> invoke_ptr >> vtable_ptr >> \
			tid >> corid))
			return false;

		if (dequeue_events.find(tid) == dequeue_events.end()) {
			cerr << "Check : missing info ";
			cerr << fixed << setprecision(1) << abstime << endl;
			return false;
		}

		dequeue_ev_t *dequeue_event = dequeue_events[tid];
		assert(dequeue_event->get_ref() == ref);
		dequeue_event->set_ptrs(func_ptr, invoke_ptr, vtable_ptr);
		dequeue_event->set_complete();
		dequeue_events.erase(tid);

		return true;
	}
	
	bool DispatchParser::new_dequeue_event(uint32_t ref, string opname,
		double abstime, istringstream &iss)
	{
		uint64_t q_id, item, ctxt, tid, coreid;
		string procname;

		if (!(iss >> hex >> q_id >> item >> ctxt >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (dequeue_events.find(tid) != dequeue_events.end()) {
			cerr << "dismatched dequeue event at ";
			cerr << fixed << setprecision(1) << abstime << endl;
		}
		//assert(dequeue_events.find(tid) == dequeue_events.end());
		dequeue_ev_t *new_dequeue = new dequeue_ev_t(abstime, opname, tid, \
			q_id, item, ctxt, ref, coreid, procname);

		if (!new_dequeue) {
			cerr << "OOM: " << __func__ << endl;
			return false;
		}

		local_event_list.push_back(new_dequeue);
		dequeue_events[tid] = new_dequeue;
		return true;
	}

	bool DispatchParser::process_dequeue(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t ref;
		if (!(iss >> hex >> ref))
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
				blockinvoke_ev_t *invokebegin = *rit;
				if (invokebegin->get_tid() == new_invoke->get_tid()
					 && invokebegin->get_func() == new_invoke->get_func()) {
					new_invoke->set_root(invokebegin);
					return true;
				}
			}
			return false;
		}
	}

	void DispatchParser::symbolize_dispatch_event(BacktraceParser *parser)
	{
		if (!parser)
			return;
		images_t *host_image = parser->get_host_image();
		if (!host_image)
			return;

		debug_data_t cur_debugger;

		cerr << "load lldb for dispatch events ..." << endl;

		if (!parser->setup_lldb(&cur_debugger, host_image))
			goto clear_debugger;

		if (dynamic_cast<blockinvoke_ev_t *>(local_event_list.front()))
			symbolize_blockinvoke(&cur_debugger);
		else if (dynamic_cast<dequeue_ev_t *>(local_event_list.front()))
			symbolize_dequeue(&cur_debugger);

		cerr << "finish dispatch event symbolization." << endl;

	clear_debugger:
		if (cur_debugger.debugger.IsValid()) {
			if (cur_debugger.cur_target.IsValid())
				cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
			lldb::SBDebugger::Destroy(cur_debugger.debugger);
		}
	}

	void DispatchParser::symbolize_dequeue(debug_data_t *cur_debugger_ptr)
	{
		list<event_t *>::iterator it;
		dequeue_ev_t *dequeue_event;
		string desc;
		bool ret;

		for (it = local_event_list.begin(); it != local_event_list.end();
			it++) {
			dequeue_event = dynamic_cast<dequeue_ev_t *>(*it);
			if (!dequeue_event || dequeue_event->get_procname() != LoadData::meta_data.host)
				//dequeue_event->get_procname().find(LoadData::meta_data.host) == string::npos)
				continue;

			desc = "_func_";
			frame_info_t frame_info = {.addr = dequeue_event->get_func_ptr()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true)
				desc.append(frame_info.symbol);
			else
				desc.append("unknown");
				
			frame_info = {.addr = dequeue_event->get_invoke_ptr()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true) {
				desc.append("_invoke_");
				desc.append(frame_info.symbol);
			}
			
			frame_info = {.addr = dequeue_event->get_ctxt()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true) {
				desc.append("_ctxt_");
				desc.append(frame_info.symbol);
			}
			dequeue_event->set_desc(desc);
			desc.clear();
		}
	}
	
	void DispatchParser::symbolize_blockinvoke(debug_data_t *cur_debugger_ptr)
	{
		list<event_t *>::iterator it;
		blockinvoke_ev_t *blockinvoke_event;
		string desc;
		bool ret;

		for (it = local_event_list.begin(); it != local_event_list.end();
			it++) {
			blockinvoke_event = dynamic_cast<blockinvoke_ev_t *>(*it);
			if (!blockinvoke_event || blockinvoke_event->get_procname() != LoadData::meta_data.host)
				//blockinvoke_event->get_procname().find(LoadData::meta_data.host) == string::npos)
				continue;

			desc = "_func_";
			frame_info_t frame_info = {.addr = blockinvoke_event->get_func()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true) {
				desc.append(frame_info.symbol);
				desc.append(frame_info.filepath);
			}
			else
				desc.append("unknown");
			
			frame_info = {.addr = blockinvoke_event->get_ctxt()};
			ret = Frames::lookup_symbol_via_lldb(cur_debugger_ptr, &frame_info);
			if (ret == true) {
				desc.append("_ctxt_");
				desc.append(frame_info.symbol);
				desc.append(frame_info.filepath);
			}
			
			blockinvoke_event->set_desc(desc);
			desc.clear();
		}
	}

	bool DispatchParser::process_blockinvoke(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t func, _ctxt, _begin, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func >> _ctxt >> _begin >> unused >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		blockinvoke_ev_t *new_invoke = new blockinvoke_ev_t(abstime, opname,
			tid, func, _ctxt, _begin, coreid, procname);
		if (!new_invoke) {
			cerr << "OOM: " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		if (checking_blockinvoke_pair(new_invoke) == false) {
			outfile << "Check: a blockinvoke end missing counterpart begin.";
			outfile << "It is normal if the invoke begins before tracing." << endl;
			delete (new_invoke);
			return false;
		}

		new_invoke->set_complete();
		local_event_list.push_back(new_invoke);
		return true;
	}

	bool DispatchParser::process_migservice(string opname, double abstime, istringstream &iss)
	{
		uint64_t unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> unused >> unused >> unused >> unused >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		disp_mig_ev_t *mig_service = new disp_mig_ev_t(abstime, opname, tid, coreid, procname);
		if (!mig_service) {
			cerr << "OOM: " << __func__ << endl;
			exit(EXIT_FAILURE);
		}
		mig_service->set_complete();
		local_event_list.push_back(mig_service);
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
					ret = process_enqueue(opname, abstime, iss);
					break;
				case DISP_DEQ:
					ret = process_dequeue(opname, abstime, iss);
					break;
				case DISP_EXE:
					ret = process_blockinvoke(opname, abstime, iss);
					break;
				case DISP_MIG:
					ret = process_migservice(opname, abstime, iss);
					break;
				default:
					ret = false;
					outfile << "unknown operation" << endl;
			}
			if (ret == false)
				outfile << line << endl;
		}
		//TODO check remaining maps
	}
}
