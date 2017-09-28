#include "parser.hpp"
#include "voucher.hpp"

namespace Parse
{
	VoucherParser::VoucherParser(string filename)
	:Parser(filename)
	{}
	
	bool VoucherParser::process_voucher_info(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t kmsg_addr, _voucher_addr, _bank_attr_type, pids, tid, coreid;
		pid_t holder_pid, merchant_pid;
		string procname = "";

		if (!(iss >> hex >> kmsg_addr >> _voucher_addr >> _bank_attr_type \
			>> pids >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		voucher_ev_t *new_voucher_info = new voucher_ev_t(abstime - 0.05,
			opname, tid, kmsg_addr, _voucher_addr,
			_bank_attr_type, coreid, procname);

		if (!new_voucher_info) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);	
		}

		holder_pid = (pid_t)(Parse::hi(pids));
		merchant_pid = (pid_t)(Parse::lo(pids));

		if (holder_pid != 0)
			new_voucher_info->set_bank_holder(holder_pid);

		if (merchant_pid != 0)
			new_voucher_info->set_bank_merchant(merchant_pid);
		
		local_event_list.push_back(new_voucher_info);
		new_voucher_info->set_complete();
		return true;
	}
	
	bool VoucherParser::process_voucher_conn(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t voucher_ori, voucher_new, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> voucher_ori >> voucher_new >> unused >> unused \
			>> tid >> coreid)) {
			return false;
		}

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		voucher_conn_ev_t  *new_voucher_conn = new voucher_conn_ev_t(abstime,
			opname, tid, voucher_ori, voucher_new, coreid, procname);
		if (!new_voucher_conn) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		local_event_list.push_back(new_voucher_conn);
		new_voucher_conn->set_complete();
		return true;
	}

	bool VoucherParser::process_voucher_transit(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t voucher_dst, voucher_src, unused, tid, coreid;
		string procname = "";

		if (!(iss >> hex >> voucher_dst >> voucher_src >> unused >> unused \
			>> tid >> coreid)) {
			return false;
		}

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		voucher_transit_ev_t *new_voucher_transit = new voucher_transit_ev_t(
			abstime, opname, tid, voucher_dst, voucher_src, coreid, procname);
		/*voucher_src is only valid on "voucher reuse"*/
		if (!new_voucher_transit) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		local_event_list.push_back(new_voucher_transit);
		new_voucher_transit->set_complete();
		return true;
	}

	bool VoucherParser::process_voucher_deallocate(string opname,
		double abstime, istringstream &iss)
	{
		uint64_t voucher, unused, tid, coreid;
		string procname = "";
		if (!(iss >> hex >> voucher >> unused >> unused >> unused \
			>> tid >> coreid)) {
			return false;
		}

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		voucher_dealloc_ev_t *dealloc_voucher_event = new voucher_dealloc_ev_t(
			abstime, opname, tid, voucher, coreid, procname);
		if (!dealloc_voucher_event) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		local_event_list.push_back(dealloc_voucher_event);
		dealloc_voucher_event->set_complete();
		return true;
	}

	bool VoucherParser::process_bank_account(string opname, double abstime,
		istringstream &iss)
	{
		uint64_t merchant, holder, unused, tid, coreid;
		string procname = "";
		if (!(iss >> std::hex >> merchant >> holder >> unused >> unused \
			>> tid >> coreid)) {
			return false;
		}

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		bank_ev_t *new_bank_event = new bank_ev_t(abstime, opname, tid,
			merchant, holder, coreid, procname);
		if (!new_bank_event) {
			cerr << "OOM " << __func__ << endl;
			exit(EXIT_FAILURE);
		}

		local_event_list.push_back(new_bank_event);
		new_bank_event->set_complete();
		return true;
	}

	/* MACH_IPC_MSG carries voucher(voucher_ev_t)
	 * Processing Command on voucher(voucher_conn_ev_t)
	 * Resule of Command is to create/reuse voucher (voucher_transit_ev_t)
	 * End of a voucher (voucher_free_ev_t)
	 */
	void VoucherParser::process(void)
	{
		string line, deltatime, opname;
		double abstime;
		bool ret = false;

		while(getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname)){
				outfile << line << endl;
				continue;
			}
			switch (LoadData::map_op_code(0, opname)) {
				case MACH_IPC_VOUCHER_INFO:
					ret = process_voucher_info(opname, abstime, iss);
					break;
				case MACH_IPC_VOUCHER_CONN:
					ret = process_voucher_conn(opname, abstime, iss);
					break;
				case MACH_IPC_VOUCHER_TRANSIT:
					ret = process_voucher_transit(opname, abstime, iss);
					break;
				case MACH_IPC_VOUCHER_DEALLOC:
					ret = process_voucher_deallocate(opname, abstime, iss);
					break;
				case MACH_BANK_ACCOUNT:
					ret = process_bank_account(opname, abstime, iss);
					break;
				default:
					ret = false;
					cerr << "unknown op code" << endl;
			}
			if (ret == false)
				outfile << line << endl;			
		}
	}
}
