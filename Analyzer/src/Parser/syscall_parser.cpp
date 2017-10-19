#include "parser.hpp"
namespace Parse
{
	SyscallParser::SyscallParser(string filename)
	:Parser(filename)
	{
		syscall_events.clear();
	}

	bool SyscallParser::new_msc(syscall_ev_t *syscall_event, uint64_t tid, string opname, uint64_t *args, int size)
	{

		string syscall_name = opname.substr(opname.find("MSC_") + 4);

		if (LoadData::msc_name_index_map.find(syscall_name)
				!= LoadData::msc_name_index_map.end()) {
			const struct syscall_entry *entry
				= &LoadData::mach_syscall_table[LoadData::msc_name_index_map[syscall_name]];
			syscall_event->set_entry(entry);
		} else {
			outfile << "Check: unrecognize syscall entry " << opname << endl;
			return false;
		}
	
		syscall_event->set_args(args, size);
		local_event_list.push_back(syscall_event);
		syscall_events[tid] = syscall_event;
		return true;
	}

	bool SyscallParser::process_msc(string opname, double abstime, bool is_end, istringstream &iss)
	{
		uint64_t args[4], tid, coreid;
		string procname = "";

		if (!(iss >> hex >> args[0] >> args[1] >> args[2] >> args[3] >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (syscall_events.find(tid) == syscall_events.end()) {
			if (is_end == true) {
				outfile << "Error: syscall missing begin part at ";
				return false;
			}
			syscall_ev_t *syscall_event = new syscall_ev_t(abstime, opname, 
					tid, MSC_SYSCALL, coreid, procname);
			if (!syscall_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}
			if (new_msc(syscall_event, tid, opname, args, 4) == false) {
				delete(syscall_event);
				return false;
			}
		} else {
			syscall_ev_t *syscall_event = syscall_events[tid];

			if (syscall_event->get_op() != opname) {
				outfile << "Error : Try to match ";
				outfile << fixed << setprecision(1) << syscall_event->get_abstime() << endl;
				outfile << "Error : with different op ";
				outfile << syscall_event->get_op() << " " << opname << endl;
				return false;
			}

			if (!is_end) {
				if (syscall_event->set_args(args, 4) == false)
					cerr << "set args for syscall at " << fixed << setprecision(1) << abstime << endl;
			} else {
				syscall_event->set_ret(args[0]);
				syscall_event->set_complete();
				syscall_event->set_ret_time(abstime);
				syscall_events.erase(tid);
			}
		}
		return true;
	}

	bool SyscallParser::new_bsc(syscall_ev_t *syscall_event, uint64_t tid, string opname, uint64_t *args, int size)
	{
		string syscall_name = opname.substr(opname.find("BSC_") + 4);
		if (LoadData::bsc_name_index_map.find(syscall_name)
				!= LoadData::bsc_name_index_map.end()) {
			const struct syscall_entry *entry
				= &LoadData::bsd_syscall_table[LoadData::bsc_name_index_map[syscall_name]];
			syscall_event->set_entry(entry);
		} else {
			outfile << "Check: unrecognize syscall entry " << opname << endl;
			return false;
		}

		syscall_event->set_args(args, size);
		local_event_list.push_back(syscall_event);
		syscall_events[tid] = syscall_event;
		return true;
	}

	bool SyscallParser::process_bsc(string opname, double abstime, bool is_end, istringstream &iss)
	{
		uint64_t args[4], tid, coreid;
		string procname = "";

		if (!(iss >> hex >> args[0] >> args[1] >> args[2] >> args[3] >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (syscall_events.find(tid) == syscall_events.end()) {
		retry:
			syscall_ev_t *syscall_event = new syscall_ev_t(abstime, opname, tid,
					BSC_SYSCALL, coreid, procname);
			if (!syscall_event) {
				cerr << "OOM " << __func__ << endl;
				exit(EXIT_FAILURE);
			}
			if (new_bsc(syscall_event, tid, opname, args, 4) == false) {
				delete(syscall_event);
				return false;
			}
		} else {
			syscall_ev_t *syscall_event = syscall_events[tid];
			/* BSD syscall may return to user space directly,
			 * according to log and code desc
			 */
			if ((syscall_event->get_op() != opname) || (!is_end && !syscall_event->audit_args_num(1))) {
				syscall_event->set_complete();
				syscall_events.erase(tid);
				outfile << "Check : " << syscall_event->get_op() << " " << opname;
				outfile << " without return record " << endl;
				outfile << fixed << setprecision(1) << syscall_event->get_abstime() << endl;
				goto retry;
			}

			if (!is_end) {
				if (syscall_event->set_args(args, 4) == false) {
					cerr << "Error: fail to set args for syscall at " << fixed << setprecision(1) << abstime << endl;
					exit(EXIT_FAILURE);
				}
			} else {
				syscall_event->set_ret(args[0]);
				syscall_event->set_complete();
				syscall_event->set_ret_time(abstime);
				syscall_events.erase(tid);
			}
		}
		return true;
	}
	
	void SyscallParser::process()
	{
		string line, deltatime, opname;
		double abstime;
		bool ret;

		while(getline(infile, line)) {
			istringstream iss(line);

			if (!(iss >> abstime >> deltatime >> opname)) {
				outfile << line << endl;
				continue;
			}

			bool is_end = deltatime.find("(") != string::npos ? true : false;
			switch (LoadData::map_op_code(0, opname)) {
				case MACH_SYS:
					ret = process_msc(opname, abstime, is_end, iss);
					break;
				case BSD_SYS:
					ret = process_bsc(opname, abstime, is_end, iss);
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
