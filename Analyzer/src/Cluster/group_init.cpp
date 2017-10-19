#include "group.hpp"
#include "thread_divider.hpp"
#include <algorithm>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define DEBUG_GROUP	1

#if DEBUG_GROUP
#include <time.h>
static time_t time_begin, time_end;
#endif

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Groups::Groups(op_events_t &_op_lists)
:op_lists(_op_lists)
{
	groups.clear();
	main_groups.clear();
	mkrun_map.clear();
	categories.clear();
	main_thread = nsevent_thread = -1;

#if DEBUG_GROUP
	time(&time_begin);
	cerr << "Begin to sort eventlists..." << endl;
#endif

	EventListOp::sort_event_list(op_lists[0]);
	tid_lists = divide_eventlist_and_mapping(op_lists[0]);

	/* recognize uimain thread and other thread with runloop object*/
	check_host_uithreads(get_list_of_op(BACKTRACE));
	check_rlthreads(get_list_of_op(RL_OBSERVER));

#if DEBUG_GROUP
	time(&time_end);
	cerr << "Finish thread sort. Time cost is " << fixed << setprecision(2) << difftime(time_end, time_begin) << " seconds" << endl;
#endif

	/* para-processing-per-list's result template in vector */
	tid_evlist_t::iterator it;
	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		map<uint64_t, group_t *> temp;
		temp.clear();
		sub_results.push_back(temp);
	}

#if DEBUG_GROUP
	time(&time_begin);
	cerr << "Begin to generate connector..." << endl;
#endif
	/* para-processing connectors */
	para_connector_generate();
#if DEBUG_GROUP
	time(&time_end);
	cerr << "Finish connection. Time cost is "  << fixed << setprecision(2) << difftime(time_end, time_begin) << " seconds" << endl;
#endif
}

Groups::~Groups(void)
{
	map<uint64_t, group_t *>::iterator it;
	group_t *cur_group;
	
	mkrun_map.clear();
	tid_lists.clear();
	sub_results.clear();

	for (it = groups.begin(); it != groups.end(); it++) {
		cur_group = it->second;
		assert(cur_group);
		delete(cur_group);
	}
	groups.clear();
}

pid_t Groups::tid2pid(uint64_t tid)
{
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].first;
	}
	cerr << "No pid found for tid " << hex << tid << endl;
	return (pid_t)-1;
}

string Groups::tid2comm(uint64_t tid)
{
	string proc_comm = "";
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].second;
	}
	cerr << "No comm found for tid " << hex << tid << endl;
	return proc_comm;
}

string Groups::pid2comm(pid_t pid)
{
	string proc_comm = "";
	map<uint64_t, pair<pid_t, string> >::iterator it;
	for (it = LoadData::tpc_maps.begin(); it !=  LoadData::tpc_maps.end(); it++) {
		if ((it->second).first == pid) {
			return (it->second).second;
		}
	}
	cerr << "No comm found for pid " << hex << pid << endl;
	return proc_comm;
}

bool Groups::interrupt_mkrun_pair(event_t *cur, list<event_t*>::reverse_iterator rit)
{
	event_t *pre = *(++rit);

	if (cur->get_event_id() != MR_EVENT)
		return false;
	
	switch (pre->get_event_id()) {
		case INTR_EVENT: {
			mkrun_ev_t *mkrun = dynamic_cast<mkrun_ev_t *>(cur);
			intr_ev_t *interrupt = dynamic_cast<intr_ev_t *>(pre);
			return mkrun->check_interrupt(interrupt);
		}
		case TSM_EVENT:
			return true;
		default:
			return false;
	}
}

void Groups::sync_mkrun_map(list<event_t *>::iterator remove_pos)
{
	mkrun_pos_t::reverse_iterator rit;
	for (rit = mkrun_map.rbegin(); rit != mkrun_map.rend(); rit++) {
		if (rit->second == remove_pos) {
			mkrun_map.erase(next(rit).base());
			break;
		}				
	}
}

bool Groups::remove_sigprocessing_events(list<event_t *> &event_list)
{
	list<event_t *>::reverse_iterator rit = event_list.rbegin();
	list<event_t *>::iterator remove_pos;
	int sigprocessing_seq_reverse[] = {MSG_EVENT, MSG_EVENT, SYSCALL_EVENT, BACKTRACE_EVENT, /*get_thread_state*/
									WAIT_EVENT, MR_EVENT, MSG_EVENT};
	int i = 0, n = sizeof(sigprocessing_seq_reverse) / sizeof(int);

	//cerr << "sizeof sigprocessing events sequence " << n << endl;
	while (rit != event_list.rend() && (*rit)->get_event_id() != MSG_EVENT)
		rit++;

	for (;rit != event_list.rend() && i < n; i++) {
		while (rit != event_list.rend()) {
			if ((*rit)->get_event_id() == INTR_EVENT)
				rit++;
			else if (interrupt_mkrun_pair((*rit), rit)) {
				rit++;
				if (rit == event_list.rend()) 
					return true;
				rit++;
			} else {
				break;
			}
		}
		
		if (rit == event_list.rend()) 
			return true;

		if (sigprocessing_seq_reverse[i] == WAIT_EVENT) {
			if ((*rit)->get_event_id() == MR_EVENT 
				&& interrupt_mkrun_pair((*rit), rit) == false) {
				cerr << "skip wait event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
				continue;
			} else if ((*rit)->get_op() == "MSC_mach_reply_port") {
				//cerr << "remove event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
				remove_pos = next(rit).base();
				sync_mkrun_map(remove_pos);
				event_list.erase(remove_pos);
				if (rit == event_list.rend()) 
					return true;
			}
		}

		if ((*rit)->get_event_id() == sigprocessing_seq_reverse[i]) {
			//cerr << "remove event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
			remove_pos = next(rit).base();
			sync_mkrun_map(remove_pos);
			event_list.erase(remove_pos);
		} else {
#if DEBUG_GROUP
			cerr << "no matching event " << i << " at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
#endif
			if (sigprocessing_seq_reverse[i] == MR_EVENT && (*rit)->get_event_id() == MSG_EVENT) {
				msg_ev_t * msg_event = dynamic_cast<msg_ev_t *>(*rit);
				if (msg_event->get_header()->check_recv() == false && msg_event->is_freed_before_deliver()) {
#if DEBUG_GROUP
					cerr << "remove event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
#endif
					remove_pos = next(rit).base();
					sync_mkrun_map(remove_pos);
					event_list.erase(remove_pos);
				}
			}
			return false;
		}
	}
	return true;
}

/* TODO: check if updates are necessary
 * filled incomplete proc comm in trace tool
 */
Groups::tid_evlist_t Groups::divide_eventlist_and_mapping(list<event_t *> &ev_list)
{
	list<event_t *>::iterator it;
	tid_evlist_t tid_list_map;
	map<uint64_t, mkrun_ev_t *> peertid_map;
	mkrun_ev_t *mr_event;
	uint64_t tid, peer_tid;

	for (it = ev_list.begin(); it != ev_list.end(); it++) {
		tid = (*it)->get_tid();
		if (tid_list_map.find(tid) == tid_list_map.end()) {
			list<event_t *> l;
			l.clear();
			tid_list_map[tid] = l;
		}
		
		/* remove events related to bsc_signal processing */
		if ((*it)->get_event_id() == BREAKPOINT_TRAP_EVENT)
			remove_sigprocessing_events(tid_list_map[tid]);
		
		tid_list_map[tid].push_back(*it);
		
		/* record causality across thread boundary via make runnable */
		if (peertid_map.find(tid) != peertid_map.end()) {
			mkrun_map[peertid_map[tid]] = std::prev(tid_list_map[tid].end());
			peertid_map.erase(tid);
		}

		if ((mr_event = dynamic_cast<mkrun_ev_t *>(*it))) {
			peer_tid = mr_event->get_peer_tid();
#if DEBUG_GROUP
			if (peertid_map.find(peer_tid) != peertid_map.end())
				cerr << "Error: multiple make runnable " \
					<< fixed << setprecision(1) \
					<< mr_event->get_abstime() << endl;
#endif
			peertid_map[peer_tid] = mr_event;
		}
	}
	return tid_list_map;
}

void Groups::check_rlthreads(list<event_t *> &rlobserver_list)
{
	list<event_t *>::iterator it;
	rl_observer_ev_t *rl_observer_event;
	uint64_t tid;

	set<uint64_t> with_observer_entry;
	set<uint64_t> wo_observer_entry;

	for (it = rlobserver_list.begin(); it != rlobserver_list.end(); it++) {
		rl_observer_event = dynamic_cast<rl_observer_ev_t *>(*it);
		tid = rl_observer_event->get_tid();

		if (!rl_observer_event)
			break;
		
		switch (rl_observer_event->get_stage()) {
			case kCFRunLoopEntry:
				with_observer_entry.insert(tid);
				break;
			case kCFRunLoopExtraEntry:
				if (with_observer_entry.find(tid) == with_observer_entry.end())
					wo_observer_entry.insert(tid);
				else
					wo_observer_entry.erase(tid);
				break;
			default:
				break;
		}
	}
	categories[RLTHR_WITH_OBSERVER_ENTRY] = with_observer_entry;
	categories[RLTHR_WO_OBSERVER_ENTRY] = wo_observer_entry;
}

void Groups::check_host_uithreads(list<event_t *> &backtrace_list)
{
	list<event_t *>::iterator it;
	backtrace_ev_t *backtrace_event;
	uint64_t tid;
	string mainthread("NSApplication run");
	string nsthread("_NSEventThread");

	for (it = backtrace_list.begin(); it != backtrace_list.end(); it++) {
		if (main_thread != -1 && nsevent_thread != -1)
			break;

		backtrace_event = dynamic_cast<backtrace_ev_t *>(*it);
		if (!backtrace_event)
			break;

		tid = (*it)->get_tid();

		if ((-1 != main_thread && tid == main_thread)
			|| (-1 != nsevent_thread && tid == nsevent_thread))
			continue;

		if (backtrace_event->get_procname() != LoadData::meta_data.host
			&& tid2comm(tid) != LoadData::meta_data.host)
			continue;

		if (main_thread == -1
			&& backtrace_event->check_backtrace_symbol(mainthread)) {
			main_thread = tid;
			continue;
		}

		if (nsevent_thread == -1
			&& backtrace_event->check_backtrace_symbol(nsthread)) {
			nsevent_thread = tid;
			continue;
		}
	}
#if DEBUG_GROUP
	cerr << "Mainthread " << hex << main_thread << endl;
	cerr << "Eventthread " << hex << nsevent_thread << endl;
#endif
}


void Groups::mr_connector_generate(void)
{
	mkrun_pos_t::iterator mkrun_it;
	for (mkrun_it = mkrun_map.begin(); mkrun_it != mkrun_map.end(); mkrun_it++) {
		mkrun_ev_t *mr_event = mkrun_it->first;
		list<event_t *> &tidlist = get_list_of_tid(mr_event->get_peer_tid());
		list<event_t *>::iterator pos = mkrun_it->second;

		while (pos != tidlist.end() && (*pos)->get_group_id() == -1)
			pos++;

		if (pos == tidlist.end())
			continue;

		/* check causality of make run event */
		if (dynamic_cast<timercallout_ev_t *>(*pos)
			|| mr_event->get_mr_type() == WORKQ_MR)
			continue;

		mr_event->set_peer_event(*pos);
	}
}

void Groups::para_connector_generate(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	/*pair mach msg*/
	msgpattern_t msg_pattern(get_list_of_op(MACH_IPC_MSG));
	ioService.post(boost::bind(&MsgPattern::collect_patterned_ipcs, msg_pattern));

	/*pair dispatch queue*/
	dispatch_pattern_t dispatch_pattern(get_list_of_op(DISP_ENQ),
			get_list_of_op(DISP_DEQ),
			get_list_of_op(DISP_EXE));
	ioService.post(boost::bind(&DispatchPattern::connect_dispatch_patterns,
				dispatch_pattern));

	/*pair timer callout*/
	timercall_pattern_t timercall_pattern(get_list_of_op(MACH_CALLCREATE),
			get_list_of_op(MACH_CALLOUT),
			get_list_of_op(MACH_CALLCANCEL));
	ioService.post(boost::bind(&TimercallPattern::connect_timercall_patterns,
				timercall_pattern));

	/*match core animation*/
	ca_connection_t ca_connection(get_list_of_op(CA_SET),
			get_list_of_op(CA_DISPLAY));
	ioService.post(boost::bind(&CAConnection::ca_connection, ca_connection));

	/*match breakpoint_trap connection*/
	breakpoint_trap_connection_t breakpoint_trap_connection(get_list_of_op(BREAKPOINT_TRAP));
	ioService.post(boost::bind(&BreakpointTrapConnection::breakpoint_trap_connection, breakpoint_trap_connection));

	/*fill wait -- makerun info*/
	mkrun_wait_t mkrun_wait_pair(get_list_of_op(MACH_WAIT),
			get_list_of_op(MACH_MK_RUN));
	ioService.post(boost::bind(&MkrunWaitPair::pair_wait_mkrun,
				mkrun_wait_pair));

	/*fill voucher info*/
	voucher_bank_attrs_t voucher_bank_attrs(get_list_of_op(MACH_BANK_ACCOUNT),
			get_list_of_op(MACH_IPC_VOUCHER_INFO));
	//get_pid_comms());
	ioService.post(boost::bind(&VoucherBankAttrs::update_event_info,
				voucher_bank_attrs));

	work.reset();
	threadpool.join_all();
	//ioService.stop();

	/*pair makerun - runnable thread*/
	mr_connector_generate();
}

void Groups::para_group(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
	
	tid_evlist_t::iterator it;
	int idx = 0;
	int main_idx = -1;

#if DEBUG_GROUP
	time(&time_begin);
	cerr << "Begin para-grouping ... " << endl;
#endif

	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		if (it->first == 0 || (it->second).size() == 0)
			continue;
		if (categories[RLTHR_WITH_OBSERVER_ENTRY].find(it->first) != categories[RLTHR_WITH_OBSERVER_ENTRY].end())  {
			RLThreadDivider rl_thread_divider(idx, sub_results, it->second, false);
			ioService.post(boost::bind(&RLThreadDivider::divide, rl_thread_divider));
			if (it->first == main_thread)
				main_idx = idx;
		} else if (categories[RLTHR_WO_OBSERVER_ENTRY].find(it->first) != categories[RLTHR_WO_OBSERVER_ENTRY].end())  {
			RLThreadDivider rl_thread_divider(idx, sub_results, it->second, true);
			ioService.post(boost::bind(&RLThreadDivider::divide, rl_thread_divider));
		} else {
			ThreadDivider general_thread_divider(idx, sub_results, it->second);
			ioService.post(boost::bind(&ThreadDivider::divide, general_thread_divider));
		}
		idx++;
	}
	
	work.reset();
	threadpool.join_all();
	//ioService.stop();
	
	vector<map<uint64_t, group_t *>>::iterator ret_it;
	for (ret_it = sub_results.begin(); ret_it != sub_results.end(); ret_it++) {
		collect_groups(*ret_it);
	}
	if (main_idx != -1)
		main_groups = sub_results[main_idx];
#if DEBUG_GROUP
	time(&time_end);
	cerr << "Finished para-grouping. Time cost is " << fixed << setprecision(2) << difftime(time_end, time_begin) << endl;
#endif
}

group_t * Groups::group_of(event_t *event)
{
	uint64_t gid = event->get_group_id();
	if (groups.find(gid) != groups.end()) 
		return groups[gid];
	return NULL;
}

void Groups::collect_groups(map<uint64_t, group_t *> &sub_groups)
{
	if (sub_groups.size())
		groups.insert(sub_groups.begin(), sub_groups.end());
}

int Groups::decode_groups(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t *>::iterator it;

	output << "Total number of Groups = " << groups.size() << endl << endl;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t * cur_group = it->second;
		output << "#Group " << hex << cur_group->get_group_id();
		output << "(length = " << hex << cur_group->get_size() << "):\n";
		cur_group->decode_group(output);
	}
	output.close();
	return 0;
}

int Groups::streamout_groups(string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t *>::iterator it;

	output << "Total number of Groups = " << groups.size() << endl;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t *cur_group = it->second;
		output << "#Group " << hex << cur_group->get_group_id();
		output << "(length = " << hex << cur_group->get_size() <<"):\n";
		cur_group->streamout_group(output);
	}
	output.close();
	return 0;
}

bool Groups::check_interleaved_pattern(list<event_t *> &ev_list,
	list<event_t *>::iterator &it)
{
	msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*it);
	assert(msg_event);
	bool found = false;
	for (it++; it != ev_list.end()
		&& (*it)->get_abstime() < msg_event->get_next()->get_abstime(); it++) {
		msg_ev_t *other = NULL;
		if ((other = dynamic_cast<msg_ev_t *>(*it)) 
			&& other->get_header()->is_mig() == false
			&& other->get_peer()
			&& other->get_peer()->get_tid() != msg_event->get_next()->get_tid()
			&& other->get_peer()->get_tid() != msg_event->get_tid()) {
			cerr << "Interleaved procs: " << msg_event->get_procname() \
				<< "\t" << other->get_peer()->get_procname() << " at " \
				<< fixed << setprecision(1) << other->get_abstime() \
				<< "\t" << msg_event->get_peer()->get_procname() << endl;
			found = true;
		}
	}
	return found;
}

void Groups::check_pattern(string output)
{
	tid_evlist_t::iterator it;
	list<event_t *>::iterator ev_it;

	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		if ((it->second).size() == 0)
			continue;
		for (ev_it = (it->second).begin(); ev_it != (it->second).end(); ev_it++) {
			msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*ev_it);
			if (!msg_event || !msg_event->get_header()->check_recv())
				continue;

			if (!msg_event->get_next())
				continue;

			if (msg_event->get_next()->get_tid() == msg_event->get_tid()
				&& check_interleaved_pattern(it->second, ev_it)) {
					//output
					cerr << "Above are interleaved patterns for " << msg_event->get_procname();
					cerr << "\treceive: "<< fixed \
						<< setprecision(1) << msg_event->get_abstime();
					cerr << "\treply: " <<fixed << setprecision(1) \
						<< msg_event->get_next()->get_abstime() << endl;
			}
		}
	}
}
