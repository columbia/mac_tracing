#include "group.hpp"
#include "thread_divider.hpp"
#include <time.h>

#define DEBUG_GROUP	0

static time_t time_begin, time_end, group_begin, group_end;

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Groups::Groups(EventLists *eventlists_ptr)
:op_lists(eventlists_ptr->get_event_lists())
{
	init_groups();
}

Groups::Groups(op_events_t &_op_lists)
:op_lists(_op_lists)
{
	init_groups();
}

void Groups::init_groups()
{
	time(&group_begin);
	groups.clear();
	main_groups.clear();
	host_groups.clear();
	categories.clear();
	main_thread = nsevent_thread = -1;

	/*1. divide event list */
#ifdef DEBUG_GROUP
	time(&time_begin);
	mtx.lock();
	cerr << "Begin to sort and divide lists..." << endl;
	mtx.unlock();
#endif
	//EventLists::sort_event_list(op_lists[0]);
	tid_lists = divide_eventlist_and_mapping(op_lists[0]);
	para_preprocessing_tidlists();

#ifdef DEBUG_GROUP
	time(&time_end);
	mtx.lock();
	cerr << "Finish sort and divide. Time cost is "  << fixed << setprecision(2) << difftime(time_end, time_begin) << " seconds" << endl;
	mtx.unlock();
#endif

	/*2. para-processing connector peers */
#ifdef DEBUG_GROUP
	time(&time_begin);
	mtx.lock();
	cerr << "Begin to generate connector..." << endl;
	mtx.unlock();
#endif
	para_connector_generate();
#ifdef DEBUG_GROUP
	time(&time_end);
	mtx.lock();
	cerr << "Finish connection. Time cost is "  << fixed << setprecision(2) << difftime(time_end, time_begin) << " seconds" << endl;
	mtx.unlock();
#endif

	/*3. para-grouping */
#ifdef DEBUG_GROUP
	time(&time_begin);
	mtx.lock();
	cerr << "Begin para-grouping ... " << endl;
	mtx.unlock();
#endif
	tid_evlist_t::iterator it;
	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		map<uint64_t, group_t *> temp;
		temp.clear();
		sub_results[it->first] = temp;
	}
	para_group();
#ifdef DEBUG_GROUP
	time(&time_end);
	mtx.lock();
	cerr << "Finished para-grouping. Time cost is " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds" << endl;
	mtx.unlock();
#endif
	time(&group_end);
	cerr << "Total time cost for generating groups and relationships " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds" << endl;
}

Groups::Groups(Groups &copy_groups)
:op_lists(copy_groups.get_op_lists())
{
	sub_results.clear();
	groups = copy_groups.get_groups();
	main_groups = copy_groups.get_main_groups();
	host_groups = copy_groups.get_host_groups();

	map<uint64_t, group_t *>::iterator it;
	group_t *this_group, *copy_group;

	for (it = groups.begin(); it != groups.end(); it++) {
		copy_group = it->second;
		this_group = new Group(*copy_group);
		groups[it->first] = this_group;
		//update main_groups and host_groups
		if (main_groups.find(it->first) != main_groups.end())
			main_groups[it->first] = this_group;
		if (host_groups.find(it->first) != host_groups.end())
			host_groups[it->first] = this_group;
	} 

#ifdef DEBUG_GROUP
	mtx.lock();
	cerr << "orig groups first group " << hex << copy_groups.get_groups().begin()->second << endl;
	cerr << "copy groups first group " << hex << groups.begin()->second << endl;
	mtx.unlock();
#endif
}

Groups::~Groups(void)
{

	tid_lists.clear();
	sub_results.clear();

	map<uint64_t, group_t *>::iterator it;
	group_t *cur_group;
	for (it = groups.begin(); it != groups.end(); it++) {
		cur_group = it->second;
		assert(cur_group);
		delete(cur_group);
	}
	groups.clear();
	main_groups.clear();
	host_groups.clear();
}

pid_t Groups::tid2pid(uint64_t tid)
{
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].first;
	}
#if DEBUG_GROUP
	mtx.lock();
	cerr << "No pid found for tid " << hex << tid << endl;
	mtx.unlock();
#endif
	return (pid_t)-1;
}

string Groups::tid2comm(uint64_t tid)
{
	string proc_comm = "";
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].second;
	}
#if DEBUG_GROUP
	mtx.lock();
	cerr << "No comm found for tid " << hex << tid << endl;
	mtx.unlock();
#endif
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
#if DEBUG_GROUP
	mtx.lock();
	cerr << "No comm found for pid " << hex << pid << endl;
	mtx.unlock();
#endif
	return proc_comm;
}

tid_evlist_t Groups::divide_eventlist_and_mapping(list<event_t *> &ev_list)
{
	list<event_t *>::iterator it;
	tid_evlist_t tid_list_map;
	map<uint64_t, mkrun_ev_t *> who_make_me_run_map;
	mkrun_ev_t *mr_event;
	uint64_t tid, peer_tid;

	for (it = ev_list.begin(); it != ev_list.end(); it++) {
		tid = (*it)->get_tid();
		if (tid_list_map.find(tid) == tid_list_map.end()) {
			list<event_t *> l;
			l.clear();
			tid_list_map[tid] = l;
		}

		/* record causality across thread boundary via make runnable */
		if (who_make_me_run_map.find(tid) != who_make_me_run_map.end()) {
			fakedwoken_ev_t *i_am_running_now = new fakedwoken_ev_t((*it)->get_abstime() - 0.01,
					"faked_woken", (*it)->get_tid(), who_make_me_run_map[tid],
					(*it)->get_coreid(), (*it)->get_procname());
			who_make_me_run_map[tid]->set_peer_event(i_am_running_now);
			tid_list_map[tid].push_back(i_am_running_now);
			ev_list.insert(it, i_am_running_now);
			who_make_me_run_map.erase(tid);
		}
		tid_list_map[tid].push_back(*it);

		if ((mr_event = dynamic_cast<mkrun_ev_t *>(*it))) {
			peer_tid = mr_event->get_peer_tid();
#ifdef DEBUG_GROUP
			mtx.lock();
			if (who_make_me_run_map.find(peer_tid) != who_make_me_run_map.end()) {
				cerr << "Warn: multiple make runnable " << fixed << setprecision(1) \
					<< mr_event->get_abstime() << endl;
				cerr << "\tPrevious make runnalbe at " << fixed << setprecision(1) \
					<< who_make_me_run_map[peer_tid]->get_abstime() << endl;
			}
			mtx.unlock();
#endif
			who_make_me_run_map[peer_tid] = mr_event;
		}
	}
	return tid_list_map;
}

void Groups::check_wqthreads(list<event_t *> &backtrace_list)
{	
	string start_wqthread("start_wqthread");
	set<uint64_t> wq_threads;
	list<event_t *>::iterator it;
	backtrace_ev_t *backtrace_event;
	uint64_t tid;

	for (it = backtrace_list.begin(); it != backtrace_list.end(); it++) {
		backtrace_event = dynamic_cast<backtrace_ev_t *>(*it);
		if (backtrace_event->check_backtrace_symbol(start_wqthread)) {
			tid = backtrace_event->get_tid();
			wq_threads.insert(tid);
		}
		categories[WQTHR] = wq_threads;
	}
}

void Groups::check_rlthreads(list<event_t *> &rlboundary_list, list<event_t *> &rlobserver_list)
{
	list<event_t *>::iterator it;
	uint64_t tid;

	rl_boundary_ev_t *rl_boundary_event;
	set<uint64_t> rl_entries;

	for (it = rlboundary_list.begin(); it != rlboundary_list.end(); it++) {
		rl_boundary_event = dynamic_cast<rl_boundary_ev_t *>(*it);
		tid = rl_boundary_event->get_tid();
		if (rl_entries.find(tid) != rl_entries.end())
			continue;
		if (rl_boundary_event->get_state() == ItemBegin)
			rl_entries.insert(tid);
	}
	categories[RLTHR] = rl_entries;

	rl_observer_ev_t *rl_observer_event;
	set<uint64_t> with_observer_entry;
	set<uint64_t> wo_observer_entry;

	for (it = rlobserver_list.begin(); it != rlobserver_list.end(); it++) {
		rl_observer_event = dynamic_cast<rl_observer_ev_t *>(*it);
		tid = rl_observer_event->get_tid();
		//rl object may be neseted in worker thread
		if (categories[WQTHR].find(tid) != categories[WQTHR].end())
			continue;
		
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
	//string mainthread("NSApplication run");
	string mainthread("[NSApplication run]");
	string mainthreadobserver("MainLoopObserver");
	string mainthreadsendevent("SendEventToEventTargetWithOptions");
	string nsthread("_NSEventThread");

	for (it = backtrace_list.begin(); it != backtrace_list.end(); it++) {
		if (main_thread != -1 && nsevent_thread != -1)
			break;

		backtrace_event = dynamic_cast<backtrace_ev_t *>(*it);

		tid = (*it)->get_tid();
		if ((-1 != main_thread && tid == main_thread)
			|| (-1 != nsevent_thread && tid == nsevent_thread))
			continue;

		if (backtrace_event->get_procname() != LoadData::meta_data.host
			&& tid2comm(tid) != LoadData::meta_data.host)
			continue;

		if (main_thread == -1) {
			if (backtrace_event->check_backtrace_symbol(mainthread)
			|| backtrace_event->check_backtrace_symbol(mainthreadobserver)
			|| backtrace_event->check_backtrace_symbol(mainthreadsendevent)) {
				main_thread = tid;
				continue;
			}
		}

		if (nsevent_thread == -1
			&& backtrace_event->check_backtrace_symbol(nsthread)) {
			nsevent_thread = tid;
			continue;
		}
	}
#ifdef DEBUG_GROUP
	mtx.lock();
	cerr << "Mainthread " << hex << main_thread << endl;
	cerr << "Eventthread " << hex << nsevent_thread << endl;
	mtx.unlock();
#endif
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

int Groups::remove_sigprocessing_events(list<event_t *> &event_list, list<event_t *>::reverse_iterator rit)
{
	list<event_t *>::iterator remove_pos;
	int sigprocessing_seq_reverse[] = {MSG_EVENT,
					MSG_EVENT,
					SYSCALL_EVENT,
					BACKTRACE_EVENT, /*get_thread_state*/
					WAIT_EVENT,
					MR_EVENT,
					MSG_EVENT}; /*wake up by cpu signal from kernel thread*/

	int i = 0, remove_count = 0, n = sizeof(sigprocessing_seq_reverse) / sizeof(int);

	while (rit != event_list.rend() && (*rit)->get_event_id() != MSG_EVENT)
		rit++;

	for (; rit != event_list.rend() && i < n; i++) {
	retry:
		while (rit != event_list.rend()) {
			if ((*rit)->get_event_id() == FAKED_WOKEN_EVENT) {
				remove_pos = next(rit).base();
				assert(*remove_pos == *rit);
				event_list.erase(remove_pos);
				remove_count++;
			} else if ((*rit)->get_event_id() == INTR_EVENT) {
				rit++;
			} else if (interrupt_mkrun_pair((*rit), rit)) {
				rit++;
				if (rit == event_list.rend()) 
					return remove_count;
				rit++;
			} else {
				break;
			}
		}
		
		if (rit == event_list.rend()) 
			return remove_count;

		if (sigprocessing_seq_reverse[i] == WAIT_EVENT) {
			if (((*rit)->get_event_id() == MR_EVENT && !interrupt_mkrun_pair((*rit), rit)) 
				|| (*rit)->get_event_id() == MSG_EVENT) {
#if DEBUG_GROUP
				mtx.lock();
				cerr << "skip wait event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
				mtx.unlock();
#endif
				continue;
			} else if ((*rit)->get_op() == "MSC_mach_reply_port") {
#if DEBUG_GROUP
				mtx.lock();
				cerr << "remove event at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
				mtx.unlock();
#endif
				remove_pos = next(rit).base();
				assert(*remove_pos == *rit);
				event_list.erase(remove_pos);
				remove_count++;
				if (rit == event_list.rend()) 
					return remove_count;
				goto retry;	
			} 
		}

		if ((*rit)->get_event_id() == sigprocessing_seq_reverse[i]) {
#if DEBUG_GROUP
			mtx.lock();
			cerr << "remove event " << i << " at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
			mtx.unlock();
#endif
			remove_pos = next(rit).base();
			assert(*remove_pos == *rit);
			event_list.erase(remove_pos);
			remove_count++;
		} else {
			// skip wake the kernel thread, remove the sent message to the kernel thread
			if (sigprocessing_seq_reverse[i] == MR_EVENT && (*rit)->get_event_id() == MSG_EVENT) {
				msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*rit);
				if (msg_event->get_header()->check_recv() == false) { //msg_event->is_freed_before_deliver() || msg_event) {
					remove_pos = next(rit).base();
					assert(*remove_pos == *rit);
					event_list.erase(remove_pos);
					remove_count++;
				} else {
#ifdef DEBUG_GROUP
					mtx.lock();
					cerr << "no matching mr event " << i << " at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
					mtx.unlock();
#endif
				}
			} else {
#ifdef DEBUG_GROUP
				mtx.lock();
				cerr << "no matching event " << i << " at " << fixed << setprecision(1) << (*rit)->get_abstime() << endl;
				mtx.unlock();
#endif
			}
			return remove_count;
		}
	}
	return remove_count;
}

void Groups::update_events_in_thread(uint64_t tid)
{
	list<event_t *>  &tid_list = tid_lists[tid]; 
	pid_t pid = tid2pid(tid);
	string proc_comm = (tid_list.front())->get_procname();

	if (proc_comm.size() == 0)
		proc_comm = tid2comm(tid);

#if DEBUG_GROUP
	mtx.lock();
	if (pid == -1)
		cerr << "Error : missing pid, tid = " << hex << tid << endl;

	if (proc_comm.size() == 0) {
		cerr << "Error : missing proc_comm, pid = " << dec << pid\
			<< " tid = " << hex << tid << endl; 
		proc_comm = "proc_" + to_string(pid);
	}
	mtx.unlock();
#endif

	list<event_t *>::iterator it;
	event_t *event;
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		event->set_pid(pid);
		if (event->get_procname().size() == 0)
			event->override_procname(proc_comm);
		
		/* remove events related to hwbr trap signal processing */
		if (event->get_event_id() == BREAKPOINT_TRAP_EVENT) {
			int dist = distance(tid_list.begin(), it);
			list<event_t *>::reverse_iterator rit(next(it));
			assert(*rit == *it);
			int removed_count = remove_sigprocessing_events(tid_list, rit);
#if DEBUG_GROUP
			mtx.lock();
			cerr << "From " << hex << tid << " at " << fixed << setprecision(1) << event->get_abstime() << " removed_count " << removed_count << endl;
			mtx.unlock();
#endif
			dist -= removed_count;
			assert(dist <= tid_list.size());
			it = tid_list.begin();
			advance(it, dist);
			assert((*it) == event);
		}
	}
#if DEBUG_GROUP
	mtx.lock();
	cerr << "in update_event thread " << hex << tid << " size " << tid_list.size() << endl;
	mtx.unlock();
#endif
}

void Groups::para_preprocessing_tidlists(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	tid_evlist_t::iterator it;
	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
#if DEBUG_GROUP
		mtx.lock();
		cerr << "thread " << hex << it->first << " size " <<  (it->second).size() << endl;
		mtx.unlock();
#endif
		ioService.post(boost::bind(&Groups::update_events_in_thread, this, it->first));
	}

	ioService.post(boost::bind(&Groups::check_host_uithreads, this, get_list_of_op(BACKTRACE)));
	ioService.post(boost::bind(&Groups::check_wqthreads, this, get_list_of_op(BACKTRACE)));
	ioService.post(boost::bind(&Groups::check_rlthreads, this, get_list_of_op(RL_BOUNDARY), get_list_of_op(RL_OBSERVER)));
	
	work.reset();
	threadpool.join_all();
	//ioService.stop();

#if DEBUG_GROUP
	mtx.lock();
	for (it = tid_lists.begin(); it != tid_lists.end(); it++)
		cerr << "After processing thread " << hex << it->first << " size " <<  (it->second).size() << endl;
	mtx.unlock();
#endif
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

	/*match breakpoint_trap connection
	breakpoint_trap_connection_t breakpoint_trap_connection(get_list_of_op(BREAKPOINT_TRAP));
	ioService.post(boost::bind(&BreakpointTrapConnection::breakpoint_trap_connection, breakpoint_trap_connection));
	*/

	/*match rl work*/
	rl_connection_t rl_connection(get_list_of_op(RL_BOUNDARY), tid_lists);
	ioService.post(boost::bind(&RLConnection::rl_connection, rl_connection));

	/*fill wait -- makerun info*/
	mkrun_wait_t mkrun_wait_pair(get_list_of_op(MACH_WAIT),
			get_list_of_op(MACH_MK_RUN));
	ioService.post(boost::bind(&MkrunWaitPair::pair_wait_mkrun,
				mkrun_wait_pair));

	/*fill voucher info*/
	voucher_bank_attrs_t voucher_bank_attrs(get_list_of_op(MACH_BANK_ACCOUNT),
			get_list_of_op(MACH_IPC_VOUCHER_INFO));
	ioService.post(boost::bind(&VoucherBankAttrs::update_event_info,
				voucher_bank_attrs));

	work.reset();
	threadpool.join_all();
	//ioService.stop();
}

group_t *Groups::group_of(event_t *event)
{
	assert(event);
	uint64_t gid = event->get_group_id();
	if (groups.find(gid) != groups.end()) { 
		return groups[gid];
	} 
	return NULL;
}

void Groups::collect_groups(map<uint64_t, group_t *> &sub_groups)
{
	if (sub_groups.size())
		groups.insert(sub_groups.begin(), sub_groups.end());
}

void Groups::para_group(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
	
	uint64_t idx = 0, main_idx = -1;
	vector<uint64_t> host_idxes;
	host_idxes.clear();

	tid_evlist_t::iterator it;

	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		if ((it->second).size() == 0)
			continue;
		idx = it->first;
		if (Groups::tid2comm(it->first) == LoadData::meta_data.host)
			host_idxes.push_back(idx);

		if (categories[RLTHR_WITH_OBSERVER_ENTRY].find(it->first) != categories[RLTHR_WITH_OBSERVER_ENTRY].end())  {
			RLThreadDivider rl_thread_divider(idx, sub_results, it->second, false);
			//rl_thread_divider.divide();
			ioService.post(boost::bind(&RLThreadDivider::divide, rl_thread_divider));
			if (it->first == main_thread)
				main_idx = idx;
		} else if (categories[RLTHR_WO_OBSERVER_ENTRY].find(it->first) != categories[RLTHR_WO_OBSERVER_ENTRY].end())  {
			RLThreadDivider rl_thread_divider(idx, sub_results, it->second, true);
			//rl_thread_divider.divide();
			ioService.post(boost::bind(&RLThreadDivider::divide, rl_thread_divider));
			
		} else if (categories[RLTHR].find(it->first) != categories[RLTHR].end()) {
			RLThreadDivider rl_thread_divider(idx, sub_results, it->second, false);
			//rl_thread_divider.divide();
			ioService.post(boost::bind(&RLThreadDivider::divide, rl_thread_divider));
		} else {
			ThreadDivider general_thread_divider(idx, sub_results, it->second);
			//general_thread_divider.divide();
			ioService.post(boost::bind(&ThreadDivider::divide, general_thread_divider));
		}
	}
	
	work.reset();
	threadpool.join_all();
	//ioService.stop();
	
	map<uint64_t, map<uint64_t, group_t *>>::iterator ret_it;
	//vector<map<uint64_t, group_t *>>::iterator ret_it;
	for (ret_it = sub_results.begin(); ret_it != sub_results.end(); ret_it++)
		collect_groups(ret_it->second);
	
	vector<uint64_t>::iterator idx_it;
	for (idx_it = host_idxes.begin(); idx_it != host_idxes.end(); idx_it++)
		host_groups.insert(sub_results[*idx_it].begin(), sub_results[*idx_it].end());

	if (main_idx != -1)
		main_groups = sub_results[main_idx];
}

group_t *Groups::spinning_group()
{
	cout << "Try to detect spinning group" << endl;
	if (nsevent_thread == -1) {
		return NULL;
	}

	double nsthread_spin_timestamp  = 0.0;
	list<event_t *> backtrace_events = get_list_of_op(BACKTRACE);
	list<event_t *>::iterator it;

	for (it = backtrace_events.begin(); it != backtrace_events.end(); it++) {
		backtrace_ev_t *backtrace = dynamic_cast<backtrace_ev_t *>(*it);
		if (backtrace->check_infected() == true) {
			cout << "Spindetect: " << hex << backtrace->get_group_id() << endl;
			nsthread_spin_timestamp = backtrace->get_abstime();
			break;
		}
	}
	
	if (nsthread_spin_timestamp > 0.0) {
		cout << "spin at " << fixed << nsthread_spin_timestamp << endl;
	    map<uint64_t, group_t *>mainthread_groups = sub_results[main_thread];
	    map<uint64_t, group_t *>::reverse_iterator rit;
		mtx.lock();
		cerr << "Main thread has " << mainthread_groups.size() << " groups" << endl;
		mtx.unlock();
		
	    for (rit = mainthread_groups.rbegin(); rit != mainthread_groups.rend(); rit++) {
			group_t *cur_group = rit->second;
			if (cur_group->get_first_event()->get_abstime() < nsthread_spin_timestamp) {
				if (cur_group->get_size() <= 2) {
					mtx.lock();
					cerr << "Spinning Group identified is too small " << hex << cur_group->get_group_id() << endl;
					mtx.unlock();
					continue;
				}
					
//#if DEBUG_GROUP
				mtx.lock();
				cerr << "Spinning at " << nsthread_spin_timestamp << endl;
				cerr << "Corresponding busy UI execution: Group #" << hex << cur_group->get_group_id() << endl;
				mtx.unlock();
//#endif
				return cur_group;
			}
	    }
	}
	cout << "Spinning group is not found" << endl;
	return NULL;
}

bool Groups::matched(group_t *target)
{
#if DEBUG_GROUP
	mtx.lock();
	cerr << "match group " << hex <<  target->get_group_id() << endl;
	mtx.unlock();
#endif
	map<uint64_t, group_t *>::iterator it;

	for (it = groups.begin(); it != groups.end(); it++) {
		if (*(it->second) == *target) {
#if DEBUG_GROUP
			mtx.lock();
			cerr << "group " << hex << target->get_group_id() << " matched" << endl;
			mtx.unlock();
#endif
			return true;
		}
	}
	return false;
}

void Groups::partial_compare(Groups *peer_groups, string proc_name, string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t *>::iterator it;
	for (it = groups.begin(); it != groups.end(); it++) {
		if (proc_name.size() != 0 && it->second->get_first_event()->get_procname() != proc_name)
			continue;
			if (!peer_groups->matched(it->second)) {
				//record to the file
				it->second->streamout_group(output);		
			}
	}
	output.close();
}

int Groups::decode_groups(string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t *>::iterator it;

	output << "Total number of Groups = " << groups.size() << endl << endl;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t *cur_group = it->second;
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
/////////////////////////////////////////////
/*checking can also be done paralelly*/
bool Groups::check_interleaved_pattern(list<event_t *> &ev_list, list<event_t *>::iterator &it)
{
	msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*it);
	assert(msg_event);
	bool found = false;
	mtx.lock();
	cerr << "Check interleaved msg receive at: "<< fixed << setprecision(1) << msg_event->get_abstime() << endl;
	mtx.unlock();
	for (it++; it != ev_list.end()
		&& (*it)->get_abstime() < msg_event->get_next()->get_abstime(); it++) {
		msg_ev_t *other = NULL;
		if ((other = dynamic_cast<msg_ev_t *>(*it)) 
			&& other->get_header()->is_mig() == false
			&& other->get_peer()
			&& other->get_peer()->get_tid() != msg_event->get_next()->get_tid()
			&& other->get_peer()->get_tid() != msg_event->get_tid()) {
			mtx.lock();
			cerr << "Interleaved procs: " << msg_event->get_procname() \
				<< "\t" << other->get_peer()->get_procname() << " at " \
				<< fixed << setprecision(1) << other->get_abstime() \
				<< "\t" << msg_event->get_peer()->get_procname() << endl;
			mtx.unlock();
			found = true;
		}
	}
	if (found == true) {
		mtx.lock();
		cerr << "Interleaved patterns for " << msg_event->get_procname();
		cerr << "\treceive: "<< fixed << setprecision(1) << msg_event->get_abstime();
		cerr << "\treply: " <<fixed << setprecision(1) << msg_event->get_next()->get_abstime() << endl;
		mtx.unlock();
	}
	return found;
}

void Groups::check_interleavemsg_from_thread(list<event_t *> &evlist)
{
	list<event_t *>::iterator ev_it;
	for (ev_it = evlist.begin(); ev_it != evlist.end(); ev_it++) {
		msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*ev_it);
		if (!msg_event || !msg_event->get_header()->check_recv())
			continue;

		if (!msg_event->get_next())
			continue;
		if (msg_event->get_next()->get_tid() == msg_event->get_tid())
			check_interleaved_pattern(evlist, ev_it);
		/*
			mtx.lock();
			cerr << "interleaved patterns for " << msg_event->get_procname();
			cerr << "\treceive: "<< fixed << setprecision(1) << msg_event->get_abstime();
			cerr << "\treply: " <<fixed << setprecision(1) << msg_event->get_next()->get_abstime() << endl;
			mtx.unlock();
		*/
	}
}

//void Groups::check_pattern(string output_path)
void Groups::check_msg_pattern()
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	//ofstream output(output_path);
	tid_evlist_t::iterator it;
	list<event_t *>::iterator ev_it;
	cerr << "Checking mach message interleave inside execution segment" << endl;
	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		if ((it->second).size() == 0)
			continue;
		ioService.post(boost::bind(&Groups::check_interleavemsg_from_thread, this, it->second));
		/*
		for (ev_it = (it->second).begin(); ev_it != (it->second).end(); ev_it++) {
			msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*ev_it);
			if (!msg_event || !msg_event->get_header()->check_recv())
				continue;

			if (!msg_event->get_next())
				continue;

			if (msg_event->get_next()->get_tid() == msg_event->get_tid()
				&& check_interleaved_pattern(it->second, ev_it, output)) {
				output << "Above are interleaved patterns for " << msg_event->get_procname();
				output << "\treceive: "<< fixed << setprecision(1) << msg_event->get_abstime();
				output << "\treply: " <<fixed << setprecision(1) << msg_event->get_next()->get_abstime() << endl;
			}
		}
		*/
	}
	work.reset();
	threadpool.join_all();
}

void Groups::check_noncausual_mkrun()
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	gid_group_map_t::iterator it;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t *cur_group = it->second;
		ioService.post(boost::bind(&Group::contains_noncausual_mk_edge, cur_group));
	}
	
	work.reset();
	threadpool.join_all();
}
