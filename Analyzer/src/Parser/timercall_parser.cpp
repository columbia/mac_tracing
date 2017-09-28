#include "parser.hpp"
#include "timer_callout.hpp"
namespace Parse
{
	TimercallParser::TimercallParser(string filename)
	:Parser(filename)
	{}

	bool TimercallParser::process_timercreate(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> \
			q_ptr >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		timercreate_ev_t *timercreate_event = new timercreate_ev_t(abstime,
			opname, tid, coreid, (void*)func_ptr,
			param0, param1, (void*)q_ptr, procname);
		local_event_list.push_back(timercreate_event);
		timercreate_event->set_complete();
		return true;
	}

	bool TimercallParser::process_timercallout(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";

		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> q_ptr \
			>> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		timercallout_ev_t *timercallout_event = new timercallout_ev_t(abstime, \
			opname, tid, coreid, (void*)func_ptr,
			param0, param1, (void*)q_ptr, procname);

		local_event_list.push_back(timercallout_event);
		timercallout_event->set_complete();
		return true;
	}

	bool TimercallParser::process_timercancel(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t func_ptr, param0, param1, q_ptr, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> func_ptr >> param0 >> param1 >> q_ptr >> tid \
			>> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		timercancel_ev_t * timercancel_event = new timercancel_ev_t(
			abstime - 0.05, opname, tid, coreid, (void*)func_ptr, param0,
			param1, (void*)q_ptr, procname);
		local_event_list.push_back(timercancel_event);
		timercancel_event->set_complete();
		return true;
	}
	
	void TimercallParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret = false;

		while (getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}
			switch (LoadData::map_op_code(0, opname)) {
				case MACH_CALLCREATE:
					ret = process_timercreate(opname, abstime, iss);
					break;
				case MACH_CALLOUT:
					ret = process_timercallout(opname, abstime, iss);
					break;
				case MACH_CALLCANCEL:
					ret = process_timercancel(opname, abstime, iss);
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
