#include "parser.hpp"
#include "backtraceinfo.hpp"

namespace Parse
{
	BacktraceParser::BacktraceParser(string filename, string _path_log)
	:Parser(filename), path_log(_path_log)
	{
		proc_images_map.clear();
		proc_backtraces_map.clear();
		backtrace_events.clear();
		images_events.clear();
		image_subpaths.clear();
	}
	
	bool BacktraceParser::try_add_path_to_image(cur_path_t * cur_path, images_t * cur_image)
	{
		string new_path = QueueOps::queue_to_string(cur_path->path);
		if (new_path.length() == 0) {
			outfile << "new path is empty" << endl;
			return false;
		}
		// The first path is the main proc path from tracing points
		if (cur_image->get_main_proc_path().length() == 0)
			//|| (new_path == cur_image->get_main_proc_path()))
			cur_image->set_main_proc(cur_path->vm_offset, new_path);
		cur_image->add_module(cur_path->vm_offset, new_path);
		return true;
	}

	bool BacktraceParser::collect_path_info(cur_path_t * cur_path, images_t * image, uint64_t vm_offset, uint64_t *addr, uint64_t size)
	{
		assert(size == 3);
		bool ret = true;
		if (vm_offset != cur_path->vm_offset && cur_path->vm_offset != uint64_t(-1)) {
			ret = try_add_path_to_image(cur_path, image);
			assert(cur_path->path.size() == 0);
		}
		cur_path->vm_offset = vm_offset;
		QueueOps::triple_enqueue(cur_path->path, addr[0], addr[1], addr[2]);
		return ret;
	}

	bool BacktraceParser::collect_image_for_proc(images_t *cur_image)
	{
		// dealwith xpcprocy and mdworker, verify all their copies are identical
		string procname = cur_image->get_procname();
		if (proc_images_map.find(procname) == proc_images_map.end()) {
			proc_images_map[procname] = cur_image;
			//proc_images_map[procname]->decode_images(outfile);
			return true;
		}
		else {
			//proc_images_map[procname]->decode_images(outfile);
			outfile << "Warning: reload image for the same proc " << procname << endl;
			//TODO: do checking here;
			return false;
		}
	}

	bool BacktraceParser::process_path(string opname, double abstime, istringstream &iss)
	{
		uint64_t vm_offset, addr[3], tid, coreid;
		string procname = "";
		bool ret = true;

		if (!(iss >> hex >> vm_offset >> addr[0] >> addr[1] >> addr[2] >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size()) {
			outfile << "Error: procname for dump libs should not be empty" << endl;
			return false;
		}

		if (images_events.find(tid) == images_events.end()) {
			assert(vm_offset == 0);
			images_t * new_image = new images_t(tid, procname);
			if (!new_image) {
				cerr << "Error: No memory for creating backtrace image paths" << endl;
				return false;
			}
			cur_path_t *cur_path = new cur_path_t;
			if (!cur_path) {
				cerr << "Error: No memory for creating backtrace image paths" << endl;
				delete(new_image);
				return false;
			}
			cur_path->vm_offset = -1;
			QueueOps::clear_queue(cur_path->path);
			image_subpaths[tid] = cur_path;
			images_events[tid] = new_image;
		} else {
			images_t * cur_image = images_events[tid];
			assert(image_subpaths.find(tid) != image_subpaths.end());
			cur_path_t * cur_path = image_subpaths[tid];
			assert(vm_offset != 0);
			if (vm_offset == 1) {
				ret = try_add_path_to_image(cur_path, cur_image);
				image_subpaths.erase(tid);
				delete(cur_path);
				if (ret == false)
					return ret;
				ret = collect_image_for_proc(cur_image);
				if (ret == false)
					delete(cur_image);
				images_events.erase(tid);
				return ret;
			}
			ret = collect_path_info(cur_path, cur_image, vm_offset, addr, 3);
		}
		return ret;
	}

	void BacktraceParser::collect_frames(backtrace_ev_t * backtrace_event, double abstime, uint64_t * frames, uint64_t size)
	{
		bool is_last_frame = false;
		int i = size;

		for (i = size - 1; i >= 0; i--) {
			if (frames[i] == 0) {
				is_last_frame = true;
				break;
			}
		}
		
		if (is_last_frame)
			size = i;

		backtrace_event->add_frames(frames, size);
		backtrace_event->override_timestamp(abstime - 0.5);
		if (is_last_frame || backtrace_event->get_size() >= backtrace_event->get_max_frames()) {
			backtrace_events.erase(backtrace_event->get_tid());
			backtrace_event->set_complete();
		}
	}

	void BacktraceParser::collect_backtrace_for_proc(backtrace_ev_t * backtrace_event, string procname)
	{
		if (proc_backtraces_map.find(procname) != proc_backtraces_map.end()) {
			proc_backtraces_map[procname].push_back(backtrace_event);
		} else {
			list<backtrace_ev_t*> l;
			l.clear();
			proc_backtraces_map[procname] = l;
			proc_backtraces_map[procname].push_back(backtrace_event);
		}
	}

	bool BacktraceParser::process_frame(string opname, double abstime, istringstream &iss)
	{
		uint64_t host_tag, frame[3], tid, coreid;
		string procname = "";

		if (!(iss >> hex >> host_tag >> frame[0] >> frame[1] >> frame[2] >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size()) {
			outfile << "Error: procname for frame should not be empty" << endl;
			outfile << fixed << setprecision(1) << abstime << endl;
			procname = LoadData::meta_data.host;
			//return false;
		}

	retry:
		if (backtrace_events.find(tid) == backtrace_events.end()) {
			if (frame[0] > TRACE_MAX) {
				outfile << "Error: not a new trace event" << endl;
				return false;
			}

			frames_t * frame_container = new frames_t(host_tag, procname, tid);
			backtrace_ev_t * backtrace_event = new backtrace_ev_t(abstime, opname, tid, frame_container, frame[0],
					host_tag, /*MSG_EVENT, */coreid, procname);
			collect_backtrace_for_proc(backtrace_event, procname);
				
			backtrace_events[tid] = backtrace_event;
			local_event_list.push_back(backtrace_event);
			collect_frames(backtrace_event, abstime, frame + 1, 2);
		} else {
			backtrace_ev_t * backtrace_event = backtrace_events[tid];
			if (backtrace_event->frame_tag() != host_tag) {
				// || abstime - backtrace_event->get_abstime() > 2) {
				backtrace_events.erase(tid);	
				backtrace_event->set_complete();
				goto retry;
			}
			collect_frames(backtrace_event, abstime, frame, 3);
		}
		return true;
	}

	bool BacktraceParser::setup_lldb(debug_data_t *debugger_data, images_t * images)
	{
		lldb::SBError err;
		if (images == NULL)
			return false;

		debugger_data->debugger = lldb::SBDebugger::Create(true, nullptr, nullptr);
		if (!debugger_data->debugger.IsValid()) {
			cerr << "Error: fail to create a debugger object" << endl;
			return false;
		}

		debugger_data->cur_target = debugger_data->debugger.CreateTarget(
			images->get_main_proc_path().c_str(), "x86_64", NULL, true, err);
		if (!err.Success() || !debugger_data->cur_target.IsValid()) {
			cerr << "Error: fail to create target\nError: " << err.GetCString() << endl;
			return false;
		}

		map<string, uint64_t> & modules = images->get_modules();
		map<string, uint64_t>::iterator it;
		for (it = modules.begin(); it != modules.end(); it++) {
			lldb::SBModule cur_module = debugger_data->cur_target.AddModule((it->first).c_str(), "x86_64-apple-macosx", NULL);
			if (!cur_module.IsValid()) {
				cerr << "Check: invalid module path " << it->first << endl;
				continue;
				//return false;
			}

			err = debugger_data->cur_target.SetSectionLoadAddress(cur_module.FindSection("__TEXT") , lldb::addr_t(it->second));	
			if (!err.Success()) {
				cerr << "Check: fail to load section for " << it->first << endl;
				cerr << "Check: " << err.GetCString() << endl;
				continue;
				//return false;
			}
			//cout << it->first << " load vm : " << hex << cur_module.FindSection("__TEXT").GetLoadAddress(debugger_data->cur_target) << endl;
		}
		return true;
	}

	/* made used of by other classes for symbolication
	*/
	images_t * BacktraceParser::get_host_image()
	{
		if (proc_images_map.find(LoadData::meta_data.host) != proc_images_map.end())
			return proc_images_map[LoadData::meta_data.host];
		else
			return NULL;
	}
	
	map<string, map<uint64_t, string> > &BacktraceParser::get_vmsymbol_map()
	{
		return image_vmsymbol_map;
	}

	/*TODO : improve symbolication speed via sacrifying space
	 * store symbols pairs <addr, symbol> per process
	 */
	void BacktraceParser::symbolize_backtraces()
	{
		map<string, images_t*>::iterator it;
		string cur_proc;
		debug_data_t cur_debugger;
		list<backtrace_ev_t *> cur_list;
		list<backtrace_ev_t *>::iterator lit;
		bool ret;

		//lldb::SBDebugger::Initialize();
		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++) {
			cur_proc = it->first;
			cerr << "proc_images for " << cur_proc << endl;
			if (cur_proc.find(LoadData::meta_data.host) == string::npos
				&& cur_proc.find("WindowServer") == string::npos)
				continue;
			cerr << "Decodeing backtrace for " << cur_proc << endl;
			ret = setup_lldb(&cur_debugger, it->second);
			if (ret == false)
				goto clear_debugger;

			cur_list = proc_backtraces_map[it->first];
			assert(cur_list.size() > 0);
			for (lit = cur_list.begin(); lit != cur_list.end(); lit++) {
				(*lit)->connect_frame_with_image(it->second);
				(*lit)->symbolize_frame(&cur_debugger, image_vmsymbol_map);
			}
			cerr << "Finished decodeing backtrace for " << cur_proc << endl;

		clear_debugger:	
			if (cur_debugger.debugger.IsValid()) {
				if (cur_debugger.cur_target.IsValid())
					cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
				lldb::SBDebugger::Destroy(cur_debugger.debugger);
			}
		}
		//lldb::SBDebugger::Terminate();
	}

	void BacktraceParser::process_path_from_log(void)
	{
		if(!path_log.size()) {
			cerr << "NO libinfo file for parsing" << endl;
			return;
		}

		ifstream path_in(path_log);
		if (path_in.fail()) {
			outfile << "No log file " << endl;
			return;
		}

		cerr << "Process backtrace log" << endl;
		string line, procname, path, unused;
		images_t *cur_image = NULL;
		bool nextline_is_main = false;
		bool ret;
		pid_t pid;
		uint64_t vm_offset;

		while(getline(path_in, line)) {
			istringstream iss(line);
			if (line.find("DumpProcess") != string::npos) {
				if (cur_image) {
					ret = collect_image_for_proc(cur_image);
					if (ret == false)
						delete(cur_image);
				}

				cur_image = NULL;
				if (!(iss >> unused >> hex >> pid) || !getline(iss >> ws, procname) || !procname.size()) {
					outfile  << "Error from path log : " << line << endl;
					continue;
				}

				cur_image = new images_t(-1, procname);
				if (cur_image == NULL) {
					outfile << "Error: No memory for creating backtrace image paths" << endl;
					return;
				}
				cur_image->set_pid(pid);
				nextline_is_main = true;
			} else {
				if (!cur_image) {
					outfile << line << endl;
					continue;
				}

				if (!(iss >> hex >> vm_offset) || !getline(iss >> ws, path) || !path.size()) {
					outfile  << "Error from path log : " << line << endl;
					continue;
				}

				cur_image->add_module(vm_offset, path);
				if (nextline_is_main) {
					cur_image->set_main_proc(vm_offset, path);
					nextline_is_main = false;
				}
			}
		}

		if (cur_image) {
			ret = collect_image_for_proc(cur_image);
			if (ret == false)
				delete(cur_image);
		}
		path_in.close();
	}

	void BacktraceParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret = false;

		process_path_from_log();
		while(getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}
			if (opname == "MSG_Pathinfo") {
				ret = process_path(opname, abstime, iss);
			}
			else if (opname == "MSG_Backtrace") {
				ret = process_frame(opname, abstime, iss);
			} else {
				ret = false;
				outfile << "Error: unknown operation" << endl;
			}
			if (ret == false)
				outfile << line << endl;
		}
		// symbolicated backtrace events
		symbolize_backtraces();
		// clear images in proc_images_map
	}
	
	void BacktraceParser::clear()
	{
		map<string, images_t*>::iterator it; 
		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++) {
			images_t * cur_image = it->second;
			delete(cur_image);
		}
		proc_images_map.clear();
	}
}
