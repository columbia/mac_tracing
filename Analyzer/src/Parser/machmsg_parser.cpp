#include "parser.hpp"
#include "mach_msg.hpp"

namespace Parse
{
	#define MACH_IPC_kmsg_free			0
	#define MACH_IPC_msg_trap			1
	#define MACH_IPC_msg_link			2
	#define MACH_IPC_msg_send			3
	#define MACH_IPC_msg_recv			4
	//#define MACH_IPC_msg_boarder		5
	//#define MACH_IPC_msg_thread_voucher 6
	//#define MACH_IPC_msg_dump			7
	//#define MACH_IPC_msg_dsc			8
	//#define MACH_IPC_msg_idsc			9
	
	MachmsgParser::MachmsgParser(string filename)
	:Parser(filename)
	{
		msg_events.clear();
		collector.insert(pair<string, uint64_t>("MACH_IPC_kmsg_free", MACH_IPC_kmsg_free));
		collector.insert(pair<string, uint64_t>("MACH_IPC_msg_trap", MACH_IPC_msg_trap));
		collector.insert(pair<string, uint64_t>("MACH_IPC_msg_link", MACH_IPC_msg_link));
		collector.insert(pair<string, uint64_t>("MACH_IPC_msg_send", MACH_IPC_msg_send));
		collector.insert(pair<string, uint64_t>("MACH_IPC_msg_recv", MACH_IPC_msg_recv));
		collector.insert(pair<string, uint64_t>("MACH_IPC_msg_recv_voucher_re", MACH_IPC_msg_recv));
		//collector.insert(pair<string, uint64_t>("MACH_IPC_msg_boarder", MACH_IPC_msg_boarder));
		//collector.insert(pair<string, uint64_t>("MACH_IPC_msg_thread_voucher", MACH_IPC_msg_thread_voucher));
		//collector.insert(pair<string, uint64_t>("MACH_IPC_msg_dump", MACH_IPC_msg_dump));
		//collector.insert(pair<string, uint64_t>("MACH_IPC_msg_dsc", MACH_IPC_msg_dsc));
		//collector.insert(pair<string, uint64_t>("MACH_IPC_msg_idsc", MACH_IPC_msg_idsc));
	}

	bool MachmsgParser::process_kmsg_end(string opname, double abstime, istringstream &iss)
	{
		uint64_t kmsg_addr, user_addr, rcv_or_send/* 1 send, 2 recv*/, unused, tid, coreid;
		string procname;

		if (!(iss >> hex >> kmsg_addr >> user_addr >> rcv_or_send >> unused >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		
		if (msg_events.find(tid) != msg_events.end()) {
			if (msg_events[tid]->get_header()->check_recv() == bool(rcv_or_send - 1)) {
				msg_events[tid]->set_user_addr(user_addr);
				msg_events[tid]->set_complete();
				msg_events.erase(tid);
				return true;
			} else {
				outfile << "msg type (send/rcv) does not match" << endl;
				return false;
			}
		}
		outfile << "No msg found to collect info from MACH_IPC_trap" << endl;
		return false;
	}

	bool MachmsgParser::process_kmsg_set_info(string opname, double abstime, istringstream &iss)
	{
		uint64_t kmsg_addr, portnames, remote_port, local_port, tid, coreid;
		string procname;

		if (!(iss >> hex >> kmsg_addr >> portnames >> remote_port >> local_port >> tid >> coreid))
			return false;
		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (msg_events.find(tid) != msg_events.end()) {
			msg_events[tid]->get_header()->set_port_names(Parse::hi(portnames), Parse::lo(portnames));
			msg_events[tid]->get_header()->set_remote_local_ports(remote_port, local_port);
			return true;
		}
		outfile << "No msg found to collect info from MACH_IPC_trap" << endl;
		return false;
	}

	msg_ev_t * MachmsgParser::new_msg(string msg_op_type, uint64_t tag, double abstime, uint64_t tid, uint64_t coreid, string procname)
	{
		msgh_t * msgh_info = new msgh_t;
		if (!msgh_info)
			return NULL;

		if (msg_op_type.find("recv") != string::npos)
			msgh_info->set_recv();
		
		msg_ev_t * new_msg = new msg_ev_t(abstime, msg_op_type, tid, msgh_info, (uint32_t)coreid, procname);
		if (!new_msg) {
			delete msgh_info;
			return NULL;
		}
		new_msg->set_tag(tag);
		msg_events[tid] = new_msg;
		local_event_list.push_back(new_msg);
		return new_msg;
	}

	/* Check if current msg is kern_service recv,
	 * if yes, match corresponding send to recv
	 */
	msg_ev_t * MachmsgParser::check_mig_recv_by_send(msg_ev_t * cur_msg, uint64_t msgh_id)
	{
		if (cur_msg->get_header()->check_recv() == false
			|| (LoadData::mig_dictionary.find(msgh_id - 100) == LoadData::mig_dictionary.end()))
			return NULL;

		msg_ev_t * msg;
		list<event_t*>::reverse_iterator rit = local_event_list.rbegin();
		assert(*rit == cur_msg);
		
		for (++rit; rit != local_event_list.rend(); rit++ ) {
			msg = dynamic_cast<msg_ev_t*>(*rit);
			if (msg->is_freed_before_deliver() == false && msg->get_tid() == cur_msg->get_tid()) {
				break;
			}
		}

		if (rit == local_event_list.rend()) {
			outfile << "Error : No send for recv [list end] " << fixed << setprecision(1) << cur_msg->get_abstime() << endl;
			return NULL;
		}

		/* this is a recv msg with validate mig number and find the nearest prev msg */
		if (msg->get_header()->is_mig() == false) {
			outfile << "Error : No mig send for recv [is_mig] " << fixed << setprecision(1) << cur_msg->get_abstime() << endl;
			return NULL;
		}

		if (msg->get_header()->check_recv()) {
			outfile << "Error : No send for mig recv [recv] " << fixed << setprecision(1) << cur_msg->get_abstime() << endl;
			return NULL;
		}

		return msg;
	}

	bool MachmsgParser::process_kmsg_begin(string opname, double abstime, istringstream &iss)
	{
		uint64_t kmsg_addr, msgh_bits, msgh_id, msg_voucher, tid, coreid;
		string procname;
		msg_ev_t * new_msg_event, * mig_send;
		msgh_t * header;

		if (!(iss >> hex >> kmsg_addr >> msgh_bits >> msgh_id >> msg_voucher >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";

		if (msg_events.find(tid) == msg_events.end()) {
			if (!(new_msg_event = new_msg(opname, kmsg_addr, abstime, tid, coreid, procname))) {
				cerr << "Error : Out of memory" << endl;
				return false;
			}
			header = new_msg_event->get_header();

			/* Check if current msg is kern_service recv,
			 * if yes, match corresponding send to recv
			 */
			mig_send = check_mig_recv_by_send(new_msg_event, msgh_id);
			if (header->set_msgh_id(msgh_id, mig_send) == false && mig_send)
				outfile << "Error: Incorrect mig number, not a kern_service recv " << fixed << setprecision(1) << abstime << endl;

			header->set_msgh_bits(msgh_bits);
			header->set_carried_vport(msg_voucher);
			return true;
		} else {
			if (msg_events[tid]->get_tag() == kmsg_addr)
				outfile << "Error: multiple msg operation on one kmsg_addr " << hex << kmsg_addr << endl;
			else
				outfile << "Error: single thread sends out multiple kmsg concurrently " << hex << msg_events[tid]->get_tag() << endl;
		}

		return false;
	}
	
	bool MachmsgParser::process_kmsg_free(string opname, double abstime, istringstream &iss)
	{
		uint64_t kmsg_addr, unused, tid, coreid;
		string procname;
		msg_ev_t * msg_event = NULL;

		if (!(iss >> hex >> kmsg_addr >> unused >> unused >> unused >> tid >> coreid))
			return false;

		if (!getline(iss >> ws, procname) || !procname.size())
			procname = "";
		
		list<event_t*>::reverse_iterator rit;
		for (rit = local_event_list.rbegin(); rit != local_event_list.rend(); rit++) {
			msg_event = dynamic_cast<msg_ev_t*>(*rit);
			if (msg_event->get_tag() == kmsg_addr)
					break;
		}
		
		if (rit != local_event_list.rend()) {
			if (msg_events.find(msg_event->get_tid()) != msg_events.end()) {
				if (msg_events[msg_event->get_tid()] == msg_event) {
					/* checking if kmsg_free in the middle of msg_send/recv
				 	 * get freed before successfully send/recv
				 	 */
					outfile << "Check mach_msg: msg interrupted " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
					msg_events.erase(msg_event->get_tid());
					local_event_list.remove(msg_event);
					delete(msg_event);
					return false;
				}
			} 
			/* if send -> freed then, the msg is freed before delivered
			 * otherwise it should be freed after recv by receiver
			 * TODO : check if msg freed in the middle of recv, then sender is not able to find peer
			 *        or receiver will retry
			 * observed case : a send and wake up b, b wake up c, c free msg
			 */
			if ((msg_event->get_header()->check_recv() == false ) && (msg_event->get_header()->is_mig() == false)) {
				outfile << "Check mach_msg: msg free after send " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
				//local_event_list.remove(msg_event);
				//delete(msg_event);
				msg_event->set_freed();
				return true;
			}
		}

		return true;
	}

	void MachmsgParser::process()
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
			ret = false;
			switch  (collector[opname]) {
				case MACH_IPC_kmsg_free:
					ret = process_kmsg_free(opname, abstime, iss);
					break;
				case MACH_IPC_msg_recv:
					opname = "MACH_IPC_msg_recv";
				case MACH_IPC_msg_send:
					ret = process_kmsg_begin(opname, abstime, iss);
					break;
				case MACH_IPC_msg_trap:
					ret = process_kmsg_set_info(opname, abstime, iss);
					break;
				case MACH_IPC_msg_link:
					ret = process_kmsg_end(opname, abstime, iss);
					break;
				default:
					outfile << "Error: unknown opname " << endl;
					ret = false;
			}
			if (ret == false) {
				outfile << line << endl;
			}
		}
		//TODO:checking remainding event map
	}
}
