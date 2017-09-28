#include "backtraceinfo.hpp"

Images::Images(uint64_t _tid, string _procname)
{
	procname = _procname;
	pid = -1;
	tid = _tid;
	main_proc = {.path = "", .vm_offset = uint64_t(-1)};
	modules_loaded_map.clear();
}

void Images::set_pid(pid_t _pid)
{
	if (pid == -1) 
		pid = _pid;
	if (pid != _pid)
		cerr << "Error: faile to override pid for " << procname << endl;
}

string Images::search_path(uint64_t vm_addr)
{
	//find path offset that is most near and less than vm_addr
	
	map<string, uint64_t>::iterator it;
	uint64_t max_less = 0;
	string path = "";
	for (it = modules_loaded_map.begin(); it != modules_loaded_map.end(); it++) {
		if (it->second <= vm_addr && it->second > max_less) {
			max_less = it->second;
			path = it->first;
		}
	}
	return path;
}

void Images::add_module(uint64_t vm_offset, string path)
{
	if (path != main_proc.path
		&& modules_loaded_map.find(path) != modules_loaded_map.end()) {
		cerr << "Warn: reload identical libraries path:";
		cerr << "\n\tlibpath: " << path;
		cerr << "\n\tprocess: " << procname << endl;
	}
	modules_loaded_map[path] = vm_offset;
}

void Images::decode_images(ofstream &outfile)
{
	outfile << procname << "\n"; 
	outfile << "\nTarget Path: " << main_proc.path << " load_vm: " << hex << main_proc.vm_offset << endl;
	map<string, uint64_t>::iterator it;
	if (modules_loaded_map.size())
		outfile << "Libs : \n";
	for (it = modules_loaded_map.begin(); it != modules_loaded_map.end(); it++) {
		outfile << "\t" << it->first << " load_vm: " << it->second << endl;
	}
}
