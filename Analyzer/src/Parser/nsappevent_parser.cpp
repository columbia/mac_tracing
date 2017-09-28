#include "parser.hpp"
#include "nsappevent.hpp"

namespace Parse
{
	NSAppEventParser::NSAppEventParser(string filename)
	:Parser(filename)
	{
		nsappevent_events.clear();
	}

	void NSAppEventParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t tag, keycode, unused, tid, coreid;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> tag >> keycode >> unused >> unused) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}

			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 

			if (opname == "NSEvent") {
				if (nsappevent_events.find(tid) != nsappevent_events.end()){

					nsappevent_events[tid]->set_event(tag, keycode);
					nsappevent_events[tid]->set_complete();
					nsappevent_events.erase(tid);
				} else {
					outfile <<"No nsapplication event for matching" << endl;
					outfile << line << endl;
				}
			}
			
			if (opname == "NSAppGetEvent") {
				nsapp_event_ev_t *nsapp_event_event = new nsapp_event_ev_t(abstime, opname, tid, tag, coreid, procname);
				if (!nsapp_event_event) {
					cerr << "OOM " << __func__ << endl;
					exit(EXIT_FAILURE);
				}
				nsappevent_events[tid] = nsapp_event_event;
				local_event_list.push_back(nsapp_event_event);
			}
			//nsapp_event_event->set_complete();
		}
	} 
}
