#include "parser.hpp"
#include "backtraceinfo.hpp"

#define DEBUG_PARSER 0

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
	
	bool BacktraceParser::try_add_path_to_image(cur_path_t *cur_path,
		images_t *cur_image)
	{
		string new_path = QueueOps::queue_to_string(cur_path->path);
		if (new_path.length() == 0) {
			outfile << "new path is empty" << endl;
			return false;
		}
		// The first path is the main proc path from tracing points
		if (cur_image->get_main_proc_path().length() == 0)
			cur_image->set_main_proc(cur_path->vm_offset, new_path);
		cur_image->add_module(cur_path->vm_offset, new_path);
		return true;
	}

	bool BacktraceParser::collect_path_info(cur_path_t *cur_path,
		images_t *image, uint64_t vm_offset, uint64_t *addr, uint64_t size)
	{
		assert(size == 3);

		if (vm_offset != cur_path->vm_offset &&
			cur_path->vm_offset != uint64_t(-1)) {
			try_add_path_to_image(cur_path, image);
			assert(cur_path->path.size() == 0);
		}

		cur_path->vm_offset = vm_offset;
		QueueOps::triple_enqueue(cur_path->path, addr[0], addr[1], addr[2]);

		return true;
	}

	bool BacktraceParser::collect_image_for_proc(images_t *cur_image)
	{
		string procname = cur_image->get_procname();
		pid_t pid = cur_image->get_pid();
		pair<pid_t, string> key = make_pair(pid, procname);
		if (proc_images_map.find(key) == proc_images_map.end()) {
			proc_images_map[key] = cur_image;
			return true;
		}
		outfile << "Check reload image for " << procname << "\tpid=" << hex << pid << endl;
		return false;
	}

	bool BacktraceParser::process_path(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t vm_offset, addr[3], tid, coreid;
		string procname = "";
		bool ret = true;

		if (!(iss >> hex >> vm_offset >> addr[0] >> addr[1] >> addr[2] \
			>> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size()) {
			outfile << "Error: missing procname for lib info" << endl;
			return false;
		}

		if (images_events.find(tid) == images_events.end()) {
			assert(vm_offset == 0);
			images_t *new_image = new images_t(tid, procname);
			if (!new_image) {
				cerr << "OOM " << __func__ << endl;
				return false;
			}
			if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
				new_image->set_pid(LoadData::tpc_maps[tid].first);
			} else {
				cerr << "Missing pid info " << __func__ << endl;
				return false;
			}
			
			cur_path_t *cur_path = new cur_path_t;
			if (!cur_path) {
				cerr << "OOM " << __func__ << endl;
				delete(new_image);
				return false;
			}
			cur_path->vm_offset = -1;
			QueueOps::clear_queue(cur_path->path);
			image_subpaths[tid] = cur_path;
			images_events[tid] = new_image;
		} else {
			assert(image_subpaths.find(tid) != image_subpaths.end());
			assert(vm_offset != 0);
			images_t *cur_image = images_events[tid];
			cur_path_t *cur_path = image_subpaths[tid];

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

	void BacktraceParser::collect_frames(backtrace_ev_t *backtrace_event,
		double abstime, uint64_t *frames, uint64_t size)
	{
		int i = size - 1;
		while(frames[i] == 0 && i > 0)
			i--;
		bool is_last_frame = i != size - 1 ? true : false;
		/*
		for (i = size - 1; i >= 0; i--) {
			if (frames[i] == 0) {
				is_last_frame = true;
				break;
			}
		}
		*/
		size = frames[i] == 0 ? 0 : i + 1;
		backtrace_event->add_frames(frames, size);
		backtrace_event->override_timestamp(abstime - 0.5);
		if (is_last_frame ||
			backtrace_event->get_size() >= backtrace_event->get_max_frames()) {
			backtrace_events.erase(backtrace_event->get_tid());
			backtrace_event->set_complete();
		}
	}

	void BacktraceParser::collect_backtrace_for_proc(
		backtrace_ev_t *backtrace_event, string procname)
	{
		uint64_t tid = backtrace_event->get_tid();
		if (LoadData::tpc_maps.find(tid) == LoadData::tpc_maps.end()) {
#if DEBUG_PARSER
			cerr << "unknown pid for tid " << hex << tid << " at " << __func__ << endl;
#endif
			return;
		}

		pair<pid_t, string> key = LoadData::tpc_maps[tid]; //make_pair(LoadData::tpc_maps[tid].first, procname);
		if (proc_backtraces_map.find(key) == proc_backtraces_map.end()) {
			list<backtrace_ev_t *> l;
			l.clear();
			proc_backtraces_map[key] = l;
		}
		proc_backtraces_map[key].push_back(backtrace_event);
	}

	bool BacktraceParser::process_frame(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t host_tag, frame[3], tid, coreid;
		string procname = "";

		if (!(iss >> hex >> host_tag >> frame[0] >> frame[1] >> frame[2] \
			>> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size()) {
			outfile << "Error: procname for frame should not be empty at";
			outfile << fixed << setprecision(1) << abstime << endl;
			procname = "";
		}
		
	retry:
		if (backtrace_events.find(tid) == backtrace_events.end()) {
			if (frame[0] > TRACE_MAX) {
				outfile << "Error: not a new trace event" << endl;
				return false;
			}

			frames_t *frame_container = new frames_t(host_tag, procname, tid);
			if (!frame_container) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}

			backtrace_ev_t * backtrace_event = new backtrace_ev_t(abstime,
					opname, tid, frame_container, frame[0],
					host_tag, /*MSG_EVENT, */coreid, procname);
			if (!backtrace_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}

			collect_backtrace_for_proc(backtrace_event, procname);
			backtrace_events[tid] = backtrace_event;
			local_event_list.push_back(backtrace_event);
			collect_frames(backtrace_event, abstime, frame + 1, 2);
		} else {
			backtrace_ev_t *backtrace_event = backtrace_events[tid];
			if (backtrace_event->frame_tag() != host_tag) {
				backtrace_events.erase(tid);	
				backtrace_event->set_complete();
				goto retry;
			}
			collect_frames(backtrace_event, abstime, frame, 3);
		}
		return true;
	}

	bool BacktraceParser::setup_lldb(debug_data_t *debugger_data,
		images_t *images)
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
			images->get_main_proc_path().c_str(),
			LoadData::meta_data.host_arch.c_str(), /*"x86_64"*/
			NULL, true, err);

		if (!err.Success() || !debugger_data->cur_target.IsValid()) {
			cerr << "Error: fail to create target" << endl;
			cerr << "\t" << err.GetCString() << endl;
			return false;
		}
		
		string platform;
		if (LoadData::meta_data.host_arch.find("i386") != string::npos)
			platform = "i386-apple-macosx";
		else
			platform = "x86_64-apple-macosx";
		cerr << "platform for lldb " << platform << endl;

		map<string, uint64_t> & modules = images->get_modules();
		map<string, uint64_t>::iterator it;
		for (it = modules.begin(); it != modules.end(); it++) {
			lldb::SBModule cur_module = debugger_data->cur_target.AddModule(
				(it->first).c_str(), platform.c_str(), NULL);

			if (!cur_module.IsValid()) {
				cerr << "Check: invalid module path " << it->first << endl;
				continue;
			}

			err = debugger_data->cur_target.SetSectionLoadAddress(
				cur_module.FindSection("__TEXT") , lldb::addr_t(it->second));	

			if (!err.Success()) {
				cerr << "Check: fail to load section for " << it->first << endl;
				cerr << "Check: " << err.GetCString() << endl;
				continue;
			}
			/* TODO : add DATA section to symbolicate watched variables
			err = debugger_data->cur_target.SetSectionLoadAddress(
				cur_module.FindSection("__DATA") , lldb::addr_t(it->second));	

			if (!err.Success()) {
				cerr << "Check: fail to load section for " << it->first << endl;
				cerr << "Check: " << err.GetCString() << endl;
				continue;
			}
			*/
		}
		return true;
	}

	// made used of by other classes for symbolication
	images_t *BacktraceParser::get_host_image()
	{
		map<pair<pid_t, string>, images_t *>::iterator it;
		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++) {
			if ((it->first).second == LoadData::meta_data.host)
				return it->second;
		}
		return NULL;
	}

	images_t *BacktraceParser::get_image_for_proc(pid_t pid, string procname)
	{
		map<pair<pid_t, string>, images_t *>::iterator it;
		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++) {
			if ((it->first).second == procname)// && (it->first).first == pid)
				return it->second;
		}
		return NULL;
	}
	
	map<string, map<uint64_t, string> > &BacktraceParser::get_vmsymbol_map()
	{
		return image_vmsymbol_map;
	}

	/* TODO : speedup symbolication via
	 * saveing maps <addr, symbol> per process
	 */
	void BacktraceParser::symbolize_backtraces()
	{
		map<pair<pid_t, string>, images_t*>::iterator it;
		string cur_proc;
		debug_data_t cur_debugger;
		list<backtrace_ev_t *> cur_list;
		list<backtrace_ev_t *>::iterator lit;
		bool ret;

		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++) {
			cur_proc = (it->first).second;
#if DEBUG_PARSER
			cerr << "Get proc_images and decode backtrace for " << cur_proc << endl;
#endif
			ret = setup_lldb(&cur_debugger, it->second);
			if (ret == false)
				goto clear_debugger;

			cur_list = proc_backtraces_map[it->first];
#if DEBUG_PARSER
			cerr << "backtrace list of " << (it->first).second << " pid = " << (it->first).first << ", size = " << cur_list.size() << endl;
#endif

			if (!cur_list.size())
				goto clear_debugger;

			for (lit = cur_list.begin(); lit != cur_list.end(); lit++) {
				(*lit)->connect_frame_with_image(it->second);
				(*lit)->symbolize_frame(&cur_debugger, image_vmsymbol_map);
			}
#if DEBUG_PARSER
			cerr << "Finished decodeing backtrace for " << cur_proc << " [" << (it->first).first << "]" << endl;
#endif

		clear_debugger:	
			if (cur_debugger.debugger.IsValid()) {
				if (cur_debugger.cur_target.IsValid())
					cur_debugger.debugger.DeleteTarget(cur_debugger.cur_target);
				lldb::SBDebugger::Destroy(cur_debugger.debugger);
			}
		}
	}

	void BacktraceParser::process_path_from_log(void)
	{
		if(!path_log.size()) {
			cerr << "No libinfo file for parsing" << endl;
			return;
		}

		ifstream path_in(path_log);
		if (path_in.fail()) {
			outfile << "No log file " << endl;
			return;
		}

#if DEBUG_PARSER
		cerr << "Process backtrace log " << path_log  << endl;
#endif
		string line, procname, path, arch, unused;
		images_t *cur_image = NULL;
		bool nextline_is_main = false;
		pid_t pid;
		uint64_t vm_offset;

		while(getline(path_in, line)) {
			istringstream iss(line);
			if (line.find("DumpProcess") != string::npos) {
				if (cur_image && !collect_image_for_proc(cur_image))
					delete(cur_image);

				cur_image = NULL;
				if (!(iss >> unused >> hex >> pid >> arch)
					|| !getline(iss >> ws, procname)
					|| !procname.size()) {
					outfile  << "Error from path log : " << line << endl;
					continue;
				}

				cerr << "create image for proc " << procname << endl;

				if (arch.find("i386") != string::npos)
					LoadData::meta_data.host_arch = "i386";
				else
					LoadData::meta_data.host_arch = "x86_64";

				cur_image = new images_t(-1, procname);
				if (!cur_image) {
					cerr << "OOM " << __func__ << endl;
					exit(EXIT_FAILURE);
				}
				cur_image->set_pid(pid);
				nextline_is_main = true;
			} else {
				if (!cur_image) {
					outfile << "Error no image " << line << endl;
					continue;
				}

				if (!(iss >> hex >> vm_offset)
					|| !getline(iss >> ws, path)
					|| !path.size()) {
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

		if (cur_image && !collect_image_for_proc(cur_image))
			delete(cur_image);
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
			} else if (opname == "MSG_Backtrace") {
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
		map<pair<pid_t,string>, images_t*>::iterator it; 
		for (it = proc_images_map.begin(); it != proc_images_map.end(); it++)
			delete(it->second);
		proc_images_map.clear();

		map<uint64_t, images_t *>::iterator image_it;
		for (image_it = images_events.begin(); image_it != images_events.end();
			image_it++)
			delete(image_it->second);
		images_events.clear();

		map<uint64_t, cur_path_t *>::iterator path_it;
		for (path_it = image_subpaths.begin(); path_it != image_subpaths.end();
			path_it++) 
			delete(path_it->second);
		image_subpaths.clear();
	}
}
