#include "parser.hpp"
#include "rlobserver.hpp"

namespace Parse
{
	RLObserverParser::RLObserverParser(string filename)
	:Parser(filename)
	{}

	void RLObserverParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t rl, mode, stage, unused, tid, coreid;

		while (getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> rl >> mode >> stage >> unused) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}

			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 

			rl_observer_ev_t *rl_observer_event = new rl_observer_ev_t(abstime, opname, tid, rl, mode, stage, coreid, procname);
			if (!rl_observer_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}
			
			rl_observer_event->set_complete();
			local_event_list.push_back(rl_observer_event);
		}
	} 
}
