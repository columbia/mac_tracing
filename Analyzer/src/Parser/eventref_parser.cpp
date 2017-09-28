#include "parser.hpp"
#include "eventref.hpp"

namespace Parse
{
	EventRefParser::EventRefParser(string filename)
	:Parser(filename)
	{}

	void EventRefParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t event_addr, event_class, event_kind, keycode, tid, coreid;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> event_addr >> event_class >> event_kind >> keycode) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}

			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 

			event_ref_ev_t *event_ref_event = new event_ref_ev_t(abstime, opname, tid, event_addr, event_class, event_kind, keycode, coreid, procname);
			if (!event_ref_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}
			
			event_ref_event->set_complete();
			local_event_list.push_back(event_ref_event);
		}
	} 
}
