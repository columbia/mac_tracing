#include "parser.hpp"

Parse::Parser::Parser(std::string _filename)
{
    filename = _filename;
    infile.open(filename);
    if (infile.fail()) {
        std::cerr << "Error: fail to read file " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    outfile.open(filename+".remain");
    if (outfile.fail()) {
        std::cerr << "Error: fail to write file " << filename + ".remain" << std::endl;
        infile.close();
        exit(EXIT_FAILURE);
    }
}

Parse::Parser::~Parser()
{
    long pos = outfile.tellp();
    if (infile.is_open())
        infile.close();
    if (outfile.is_open())
        outfile.close();
    local_event_list.clear();
    if (pos == 0) {
        remove(filename.c_str());
        remove((filename + ".remain").c_str());
    } else {
        std::cerr << "Warning : file " << filename << " is not parsed completely" << std::endl;
    }
}

void Parse::Parser::add_to_proc_map(Parse::key_t proc, EventBase *event)
{
	if (proc.first == -1 && proc.second == "")
		return;
	
	/*
	event_list_t event_list = get_events_for_proc(proc);
	if (event_list.size() == 0) {
		event_list.push_back(event);
		proc_event_list_map[proc] = event_list;
		return;
	}

	for (auto it = proc_event_list_map.begin(); it != proc_event_list_map.end(); it++) {
		if ((it->first).first == proc.first && (it->first).second == proc.second) {
			(it->second).push_back(event);
			proc_event_list_map[it->first] = it->second;
		}
	}
	*/
	if (proc_event_list_map.find(proc) == proc_event_list_map.end()) {
		event_list_t empty;
		empty.clear();
		proc_event_list_map[proc] = empty;
	}
	proc_event_list_map[proc].push_back(event);
}

Parse::event_list_t Parse::Parser::get_events_for_proc(Parse::key_t proc)
{
	/*
    event_list_t ret;
    ret.clear();
	for (auto it = proc_event_list_map.begin(); it != proc_event_list_map.end(); it++) {
		if ((it->first).first == proc.first && (it->first).second == proc.second)
			return it->second;
	}
	*/
    return proc_event_list_map[proc];
}
