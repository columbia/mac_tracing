#include "parser.hpp"
#include "timer_callout.hpp"
namespace Parse
{
	TimercallParser::TimercallParser(string filename)
	:Parser(filename)
	{
	}

	bool TimercallParser::process_callcreate(string opname, double abstime, istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> q_ptr >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		callcreate_ev_t * callcreate_event = new callcreate_ev_t(abstime, opname, tid, coreid, 
				(void*)func_ptr, param0, param1, (void*)q_ptr, procname);
		local_event_list.push_back(callcreate_event);
		callcreate_event->set_complete();
		return true;
	}

	bool TimercallParser::process_callout(string opname, double abstime, istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> q_ptr >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		callout_ev_t * callout_event = new callout_ev_t(abstime, opname, tid, coreid, 
				(void*)func_ptr, param0, param1, (void*)q_ptr, procname);
		local_event_list.push_back(callout_event);
		callout_event->set_complete();
		return true;
	}

	bool TimercallParser::process_callcancel(string opname, double abstime, istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> q_ptr >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		callcancel_ev_t * callcancel_event = new callcancel_ev_t(abstime - 0.05, opname, tid, coreid, 
				(void*)func_ptr, param0, param1, (void*)q_ptr, procname);
		local_event_list.push_back(callcancel_event);
		callcancel_event->set_complete();
		return true;
	}
	
	void TimercallParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret = false;
		while(getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}
			switch (LoadData::map_op_code(0, opname)) {
				case MACH_CALLCREATE:
					ret = process_callcreate(opname, abstime, iss);
					break;
				case MACH_CALLOUT:
					ret = process_callout(opname, abstime, iss);
					break;
				case MACH_CALLCANCEL:
					ret = process_callcancel(opname, abstime, iss);
					break;
				default:
					ret = false;
					cerr << "unknown op" << endl;
			}
			if (ret == false)
				outfile << line << endl;
		}
	}
}
