#include "backtraceinfo.hpp"

Frames::Frames(uint64_t tag, string procname, uint64_t _tid)
{
	host_event_tag = tag;
	proc_name = procname;
	tid = _tid;
	is_infected = false;
	frame_addrs.clear();
	frame_symbols.clear();
}

string Frames::get_sym_for_addr(uint64_t vm_offset, map<uint64_t, string> & vm_sym_map)
{
	map<uint64_t, string>::iterator next = vm_sym_map.upper_bound(vm_offset);
	if (next != vm_sym_map.begin())
		next--;
	return next->second;
}

void Frames::checking_symbol_with_image_in_memory(string &symbol, uint64_t vm, string path, map<string, map<uint64_t, string> >&image_vmsym_map)
{
	uint64_t load_addr = image->get_modules()[path];
	uint64_t vm_offset = vm - load_addr;

	//cerr << "symbol for checking " << symbol << "\t[" << hex << vm << "]\t" << path << endl;
	if (LoadData::meta_data.pid == 0) {
		//cerr << "No Symbol checking " << endl;
		return;
	}

	if (image_vmsym_map.find(path) == image_vmsym_map.end()) {
		struct hack_handler mh;
		memset(&mh, 0, sizeof(struct hack_handler));
		if (get_syms_for_libpath(LoadData::meta_data.pid, path.c_str(), &mh)) {
			uint64_t vm_offset = 0, str_index = 0;
			map<uint64_t, string> vm_syms_map;
			for (int i = 0; i < mh.nsyms; i++) {
				vm_offset = mh.symbol_arrays[i].vm_offset;
				if (vm_offset == mh.vm_slide - (uint64_t)mh.mach_address)
					break; //dysymbol stubs
				str_index = mh.symbol_arrays[i].str_index;
				vm_syms_map[vm_offset] = string(mh.strings + str_index + 1);
			}

			if (vm_syms_map.size() > 0)
				image_vmsym_map[path] = vm_syms_map;

			if (mh.strings)
				free((void *)mh.strings);

			if (mh.symbol_arrays)
				free((void *)mh.symbol_arrays);
		}
	}
	
	if (image_vmsym_map.find(path) != image_vmsym_map.end()) {
		symbol = get_sym_for_addr(vm_offset, image_vmsym_map[path]);
		//cerr << "symbol get checking " << symbol << endl;
	}
}

bool Frames::lookup_symbol_via_lldb(debug_data_t * debugger_data, frame_info_t * cur_frame)
{
	lldb::SBAddress sb_addr = debugger_data->cur_target.ResolveLoadAddress(cur_frame->addr); 
	bool success = sb_addr.IsValid() && sb_addr.GetSection().IsValid();
	if (success) {
		lldb::SBSymbolContext sc = debugger_data->cur_target.ResolveSymbolContextForAddress(sb_addr, lldb::eSymbolContextEverything);
		lldb::SBStream strm;
		sc.GetDescription(strm);
		string desc(strm.GetData());

		string filepath = "";
		string symbol = "unknown";
		size_t pos_1 = desc.find("file =");
		size_t pos_2 = desc.find(",");
		size_t pos_3 = desc.find("name=");
		size_t pos_4 = desc.find("mangled=");

		desc.erase(std::remove(desc.begin(), desc.end(), '\n'), desc.end());

		if (pos_1 != string::npos && pos_2 != string::npos) {
			filepath = desc.substr(pos_1 + 8, pos_2 - pos_1 - 9);
		}

		if (pos_3 != string::npos) {
			if (pos_4 != string::npos)
				symbol = desc.substr(pos_3 + 5, pos_4 - pos_3 - 9);
			else
				symbol = desc.substr(pos_3 + 5, desc.find("\"", pos_3 + 6) - pos_3 - 5);
		} else {
			if (pos_4 != string::npos) {
				string mangled_name = desc.substr(pos_4 + 8, desc.find("\"", pos_4 + 9) - pos_4 - 8);
				symbol = mangled_name;
			}
			else 
				symbol = desc;
		}
		cur_frame->symbol = symbol;
		cur_frame->filepath = filepath;
	}
	return success;
}

void Frames::symbolication(debug_data_t * debugger_data,  map<string, map<uint64_t, string> >&image_vmsym_map)
{
	if (proc_name.find(LoadData::meta_data.host) == string::npos
		&& proc_name.find("WindowServer") == string::npos)
		return;

	vector<uint64_t>::iterator addr_it;
	for (addr_it = frame_addrs.begin(); addr_it != frame_addrs.end(); addr_it++) {
		frame_info_t cur_frame_info = {.addr = *addr_it};

		if (lookup_symbol_via_lldb(debugger_data, &cur_frame_info)) {
			string pre_check_sym = cur_frame_info.symbol;

			if (cur_frame_info.filepath.find("CoreGraphics") != string::npos)
				checking_symbol_with_image_in_memory(cur_frame_info.symbol, *addr_it, cur_frame_info.filepath, image_vmsym_map);

			/*
			if (cur_frame_info.symbol != pre_check_sym) {
				cerr << "Different symbol from file and memory " << cur_frame_info.filepath << endl;
				cerr << "checked symbol" << cur_frame_info.symbol << endl;
				cerr << "lldb symbol" << pre_check_sym << endl;
			}
			*/
			frame_symbols.push_back(cur_frame_info.symbol + "\t" + cur_frame_info.filepath);
		
			if (cur_frame_info.symbol.find(LoadData::meta_data.suspicious_api) != string::npos) {
				cerr << "Infected [" << LoadData::meta_data.suspicious_api << "]:\t" << cur_frame_info.symbol << endl;
				is_infected = true;
			}
		} else {
			string desc("unknown_frame");
			frame_symbols.push_back(desc);
			if (*addr_it < 0xffff) {
				addr_it++;
				break;
			}
		}
	}

	for (; addr_it != frame_addrs.end(); addr_it++) {
		string desc("leaked_frame");
		frame_symbols.push_back(desc);
	}
}

void Frames::decode_frames(ofstream & outfile)
{
	if (proc_name.find(LoadData::meta_data.host) == string::npos
		&& proc_name.find("WindowServer") == string::npos)
		return;

	if (frame_addrs.size() != frame_symbols.size())
		cerr << "Error: Not symbolicated " << proc_name << endl;

	uint32_t i = 0;
	vector<uint64_t>::iterator it;
	vector<string>::iterator sit;

	for (it = frame_addrs.begin(), sit = frame_symbols.begin();
		it != frame_addrs.end() && sit != frame_symbols.end(); it++, sit++) {
		outfile << "Frame[ " << setfill('0') << setw(2) << i << " ] 0x" << setfill('0') << setw(16) << hex << *it << " : ";
		outfile << *sit << "\n";
		i++;
	}
	outfile << endl;
}

bool Frames::check_symbol(string &func)
{
	vector<string>::iterator it;
	string symbol;

	for (it = frame_symbols.begin(); it != frame_symbols.end(); it++) {
		if ((*it).find(func) != string::npos) {
			return true;
		}
	}
	return false;
}

void Frames::streamout(ofstream & outfile)
{
	if (proc_name.find(LoadData::meta_data.host) == string::npos
		&& proc_name.find("WindowServer") == string::npos)
		return;

	assert(frame_addrs.size() == frame_symbols.size());
	uint32_t i = 0;
	vector<uint64_t>::iterator it;
	vector<string>::iterator sit;
	string symbol;

	for (it = frame_addrs.begin(), sit = frame_symbols.begin();
			it != frame_addrs.end() && sit != frame_symbols.end(); it++, sit++) {
		symbol = (*sit).substr(0, (*sit).find('\t'));
		outfile << symbol << "\n";
		//outfile << *sit << "\n";
		i++;
	}
	outfile << endl;
}
