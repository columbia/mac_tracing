#include "parser.hpp"
#include "tsmaintenance.hpp"

namespace Parse
{
	TsmaintainParser::TsmaintainParser(string filename)
	:Parser(filename)
	{
	}
	
	void TsmaintainParser::process()
	{
		string line, deltatime, opname, procname;
		double abstime;
		uint64_t arg1, arg2, arg3, arg4, tid, coreid;
		while(getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname) ||
				!(iss >> hex >> arg1 >> arg2 >> arg3 >> arg4) ||
				!(iss >> tid >> coreid)) {
				outfile << line << endl;
				continue;
			}
			if (!getline(iss >> ws, procname) || !procname.size())
				procname = ""; 
			tsm_ev_t * tsm_event = new tsm_ev_t(abstime, opname, tid, coreid, procname);
			tsm_event->set_complete();
			local_event_list.push_back(tsm_event);
		}
	}
}
