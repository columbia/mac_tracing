#include "parser.hpp"
#include "caset.hpp"
#include "cadisplay.hpp"

namespace Parse
{
	CAParser::CAParser(string filename)
	:Parser(filename)
	{}

	bool CAParser::process_caset(string opname, double abstime, istringstream &iss)
	{
		uint64_t object, unused, tid, coreid;
		string procname;

		if (!(iss >> hex >> object >> unused >> unused >> unused) ||
			!(iss >> tid >> coreid)) {
			return false;
		}

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = ""; 
		
		ca_set_ev_t * ca_set_event = new ca_set_ev_t(abstime, opname, tid, object, coreid, procname);
		if (!ca_set_event) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}
		ca_set_event->set_complete();
		local_event_list.push_back(ca_set_event);
		return true;
	}

	bool CAParser::process_cadisplay(string opname, double abstime, istringstream &iss)
	{
		uint64_t object, serial, unused, tid, coreid;
		string procname;
		if (!(iss >> hex >> object >> serial >> unused >> unused) ||
			!(iss >> tid >> coreid)) {
			return false;
		}
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = ""; 

		ca_disp_ev_t *ca_display_event = new ca_disp_ev_t(abstime, opname, tid, object, coreid, procname);
		if (!ca_display_event) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}
		outfile << "serial " << hex << serial << endl;
		ca_display_event->set_serial(serial);
		outfile << "is_serial " << hex << ca_display_event->get_serial() << endl;
		ca_display_event->set_complete();
		local_event_list.push_back(ca_display_event);
		return true;
	}
	
	void CAParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret;

		while (getline(infile, line)) {
			istringstream iss(line);
			
			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}

			switch (LoadData::map_op_code(0, opname)) {
				case CA_SET:
					ret = process_caset(opname, abstime, iss);
					break;
				case CA_DISPLAY:
					ret = process_cadisplay(opname, abstime, iss);
					break;
				default:
					ret = false;	
					break;
			}
			if (ret == false)
				outfile << line << endl;	
		}
		// check events
	}
}
