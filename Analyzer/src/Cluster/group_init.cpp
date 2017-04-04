#include "group.hpp"
#include "thread_divider.hpp"
#include <algorithm>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Groups::Groups(op_events_t &_op_lists)
:op_lists(_op_lists)
{
	groups.clear();
	tid_comm.clear();
	pid_comm.clear();
	tid_pid.clear();

	mkrun_map.clear();
	main_thread = nsevent_thread = 0;
	tid_lists = divide_eventlist_and_mapping(op_lists[0]);

	/* para-processing-per-list's result template in vector */
	tid_evlist_t::iterator it;

	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		map<uint64_t, group_t*> temp;
		temp.clear();
		sub_results.push_back(temp);
	}
	update_procs_maps(LoadData::meta_data.procs_file.c_str());
	
	/* para-processing connectors */
	para_connector_generate();

	/* recognize uimain thread and nsevent thread */
	check_host_uithreads(get_list_of_op(BACKTRACE));
}

Groups::~Groups(void)
{
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	
	tid_comm.clear();
	pid_comm.clear();
	tid_pid.clear();
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
	if (tid_pid.find(tid) != tid_pid.end())
		return tid_pid[tid];
	return (pid_t)-1;
}

string Groups::tid2comm(uint64_t tid)
{
	pid_t pid = tid2pid(tid);
	string proc_comm = "";

	if (tid_comm.find(tid) != tid_comm.end()) { 
		proc_comm = tid_comm[tid];
		if (proc_comm.size()) {
			if (pid != -1 && pid_comm.find(pid) == pid_comm.end())
				pid_comm[pid] = proc_comm;
			return proc_comm;
		}
	}

	if (proc_comm.size() == 0 && pid != -1 && pid_comm.find(pid) != pid_comm.end()) {
		proc_comm = pid_comm[pid];
		tid_comm[tid] = proc_comm;
	}
	return proc_comm;
}

string Groups::pid2comm(pid_t pid)
{
	string proc_comm = "";
	if (pid_comm.find(pid) != pid_comm.end()) {
		return pid_comm[pid];
	}
	return proc_comm;
}

/* TODO: check if updates are necessary
 * filled incomplete proc comm in trace tool
 * but one proc may begin with commands like mdworks, xpcproxy...
 */
void Groups::update_procs_maps(mkrun_ev_t * mr_event)
{
	uint64_t tid = mr_event->get_tid();
	uint64_t peer_tid = mr_event->get_peer_tid();
	
	if (tid2pid(tid) == -1)
		tid_pid[tid] = mr_event->get_pid();

	if (tid2pid(peer_tid) == -1)
		tid_pid[peer_tid] = mr_event->get_peer_pid();

	tid2comm(tid);
	tid2comm(peer_tid);
}

void Groups::update_procs_maps(wait_ev_t * wait_event)
{
	uint64_t tid = wait_event->get_tid();
	if (tid2pid(tid) == -1)
		tid_pid[tid] = wait_event->get_pid();
}

void Groups::update_procs_maps(event_t * event)
{
	uint64_t tid = event->get_tid();
	string proc_name = event->get_procname();

	if (proc_name.size()) {
		tid_comm[tid] = proc_name;
		tid2comm(tid);
	}
	
	mkrun_ev_t * mr_event = dynamic_cast<mkrun_ev_t *>(event);
	if (mr_event)
		update_procs_maps(mr_event);
	wait_ev_t * wait_event = dynamic_cast<wait_ev_t *>(event);
	if (wait_event)
		update_procs_maps(wait_event);

	if (tid2pid(tid) == 0)
		assert(proc_name == "kernel_task");
}

void Groups::update_procs_maps(const char * proc_list_file)
{
	if (proc_list_file == NULL) {
		cerr << "no proc file in current dir " << endl;
		return;
	}

	ifstream input(proc_list_file);
	if (input.fail()) {
		cerr << "no proc file named " << proc_list_file << " in current directory" << endl;
		return;
	}

	string line, proc_comm;
	uint64_t pid;

	while (getline(input, line)) {
		istringstream iss(line);
		iss >> pid >> proc_comm;
		if (pid_comm.find(pid) != pid_comm.end() && pid_comm[pid] != proc_comm) {
			cerr << "multiple proc command " << pid_comm[pid] << " " << proc_comm;
			cerr << " with the same process" << dec << pid << endl;
		}
		cerr << "pid_comm [" << dec << pid << "] = " << proc_comm << endl;
		pid_comm[pid] = proc_comm;
	}
}

Groups::tid_evlist_t Groups::divide_eventlist_and_mapping(list<event_t *> &ev_list)
{
	list<event_t *>::iterator it;
	tid_evlist_t tid_list_map;
	map<uint64_t, mkrun_ev_t *> peertid_map;
	mkrun_ev_t * mr_event;
	uint64_t tid, peer_tid;

	for (it = ev_list.begin(); it != ev_list.end(); it++) {

		tid = (*it)->get_tid();

		if (tid_list_map.find(tid) == tid_list_map.end()) {
			list<event_t* > l;
			l.clear();
			tid_list_map[tid] = l;
		}
		tid_list_map[tid].push_back(*it);
		
		if (peertid_map.find(tid) != peertid_map.end()) {
			mkrun_map[peertid_map[tid]] = std::prev(tid_list_map[tid].end());
			peertid_map.erase(tid);
		}
		
		mr_event = dynamic_cast<mkrun_ev_t *>(*it);
		if (mr_event) {
			peer_tid = mr_event->get_peer_tid();
			if (peertid_map.find(peer_tid) != peertid_map.end())
				cerr << "Error: multiple make runnable " << fixed << setprecision(1) << mr_event->get_abstime() << endl;
			peertid_map[peer_tid] = mr_event;
		}

		update_procs_maps(*it);
		// TODO : fix its process name if backtrace is fabricated as host
	}

	return tid_list_map;
}

void Groups::para_connector_generate(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &ioService));

	msgpattern_t msg_pattern(get_list_of_op(MACH_IPC_MSG));
	ioService.post(boost::bind(&MsgPattern::collect_patterned_ipcs, msg_pattern));

	dispatch_pattern_t dispatch_pattern(get_list_of_op(DISP_ENQ),
										get_list_of_op(DISP_DEQ),
										get_list_of_op(DISP_EXE));
	ioService.post(boost::bind(&DispatchPattern::connect_dispatch_patterns, dispatch_pattern));

	timercall_pattern_t timercall_pattern(get_list_of_op(MACH_CALLCREATE),
										get_list_of_op(MACH_CALLOUT),
										get_list_of_op(MACH_CALLCANCEL));
	ioService.post(boost::bind(&TimercallPattern::connect_timercall_patterns, timercall_pattern));

	mkrun_wait_t mkrun_wait_pair(get_list_of_op(MACH_WAIT),
								get_list_of_op(MACH_MK_RUN));
	ioService.post(boost::bind(&MkrunWaitPair::pair_wait_mkrun, mkrun_wait_pair));

	voucher_bank_attrs_t voucher_bank_attrs(get_list_of_op(MACH_BANK_ACCOUNT),
											get_list_of_op(MACH_IPC_VOUCHER_INFO),
											get_pid_comms());
	ioService.post(boost::bind(&VoucherBankAttrs::update_event_info, voucher_bank_attrs));
	
	work.reset();
	threadpool.join_all();
	//ioService.stop();
}

void Groups::check_host_uithreads(list<event_t *> & backtrace_list)
{
	list<event_t *>::iterator it;
	backtrace_ev_t * backtrace_event;
	string mainthread("NSApplicationMain");
	string nsthread("_NSEventThread");

	for (it = backtrace_list.begin(); it != backtrace_list.end(); it++) {
		if (main_thread != 0 && nsevent_thread != 0)
			break;

		backtrace_event = dynamic_cast<backtrace_ev_t *>(*it);

		if ((main_thread != 0 && backtrace_event->get_tid() == main_thread)
			||(nsevent_thread != 0 && backtrace_event->get_tid() == nsevent_thread))
			continue;

		if (backtrace_event->get_procname() != LoadData::meta_data.host
				&& tid2comm(backtrace_event->get_tid()) != LoadData::meta_data.host)
			continue;

		if (main_thread == 0 && backtrace_event->check_backtrace_symbol(mainthread)) {
			main_thread = backtrace_event->get_tid();
			continue;
		}

		if (nsevent_thread == 0 && backtrace_event->check_backtrace_symbol(nsthread)) {
				nsevent_thread = backtrace_event->get_tid();
				continue;
		}
	}
}

group_t * Groups::create_group(uint64_t group_id, event_t *root_event)
{
	group_t * new_gptr = new group_t(group_id, root_event);
	if (root_event) 
		new_gptr->add_to_container(root_event);
	if (new_gptr == NULL)
		cerr << "Error : OOM can not create a new group." << endl;
	return new_gptr;
}

bool Groups::mr_by_intr(mkrun_ev_t * mr, intr_ev_t * intr_event) 
{
	if (intr_event == NULL)
		return false;

	double intr_begin = intr_event->get_abstime();
	double intr_end = intr_event->get_finish_time();
	double mr_time = mr->get_abstime();

	if (mr_time - intr_begin >= 10e-8 && intr_end - mr_time >= 10e-8) {
		intr_event->add_invoke_thread(mr->get_peer_tid());
		return true;
	}
	return false;
}

bool Groups::mr_by_wqnext(mkrun_ev_t * mr, event_t *last_event)
{
	wqnext_ev_t * wqnext_event = dynamic_cast<wqnext_ev_t*>(last_event);
	if (!wqnext_event)
		return false;

	double wqnext_begin = wqnext_event->get_abstime();
	double wqnext_end = wqnext_event->get_finish_time();
	double mr_time = mr->get_abstime();
	if (mr_time - wqnext_begin >= 10e-8 && wqnext_end - mr_time >= 10e-8) {
		return true;
	} 
	return false;
}

uint64_t Groups::check_mr_type(mkrun_ev_t * mr_event, event_t * last_event, intr_ev_t * potential_root)
{
	if (dynamic_cast<tsm_ev_t*>(last_event)) 
		mr_event->set_mr_type(SCHED_TSM_MR);
	else if (dynamic_cast<timercallout_ev_t *>(last_event))
		mr_event->set_mr_type(CALLOUT_MR);
	else if (mr_by_wqnext(mr_event, last_event))
		mr_event->set_mr_type(WORKQ_MR);
	else if (mr_by_intr(mr_event, potential_root))
		mr_event->set_mr_type(INTR_MR);

	return mr_event->get_mr_type();
}

group_t *Groups::add_mr_into_group(mkrun_ev_t * mr_event, group_t **cur_group_ptr, intr_ev_t * potential_root)
{
	group_t * cur_group = *cur_group_ptr;
	event_t * last_event = cur_group->get_last_event();
	uint64_t mr_type;

	if (!last_event) {
		cur_group->add_to_container(mr_event);
		return cur_group;
	}

	mr_type = check_mr_type(mr_event, last_event, potential_root);
	switch (mr_type) {
		case SCHED_TSM_MR: {
			/* processing sched_timeshare_maintainance_mkrun
	 		 * [delayed] close the cur_group and reload stored group
	 		 */
			cur_group->add_to_container(mr_event);
			tsm_ev_t * tsm = dynamic_cast<tsm_ev_t*>(last_event);
			*cur_group_ptr = ((group_t*)(tsm->load_gptr()));
			break;
		}
		case INTR_MR: { 
		/* 1. Do not remove passed in group,
		 * as it will return there after intr processing
		 * 2. Do not touch potental_root
		 * one intr may triger multiple mkrunnable
		 * 3. Check the case that an active thread being waken up again
		 */
			cur_group = create_group(-1, potential_root);
			cur_group->add_to_container(mr_event);
			break;
		}
		default:
			cur_group->add_to_container(mr_event);
	}

	return cur_group;
}

group_t * Groups::add_wait_into_group(wait_ev_t * wait_event, group_t ** cur_group_ptr)
{
	group_t * cur_group = *cur_group_ptr;
	cur_group->add_to_container(wait_event);

	/* if not in the middle of dispatch block execution
	 * close cur_group
	 * otherwise make a whole dispatch block in one group
	 */
	if (cur_group->get_blockinvoke_level() == 0)
		*cur_group_ptr = NULL;
	return cur_group;
}

group_t * Groups::add_invoke_into_group(blockinvoke_ev_t * invoke_event, group_t ** cur_group_ptr)
{
	group_t * cur_group = *cur_group_ptr;
	assert(cur_group);
	cur_group->add_to_container(invoke_event);

	if (invoke_event->is_begin()) {
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		cur_group->blockinvoke_level_inc();
		return cur_group;
	} else {
		assert(invoke_event->get_root() != NULL);
		if (cur_group->get_blockinvoke_level() <= 0) {
			cerr << "unbalanced invoke at " << fixed << setprecision(1) << invoke_event->get_abstime() << endl;
			assert(cur_group->get_first_event());
			cerr << "group begins at " << fixed << setprecision(1) << cur_group->get_first_event()->get_abstime() << endl;
		}
		assert(cur_group->get_blockinvoke_level() > 0);
		cur_group->blockinvoke_level_dec();
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());

		if (cur_group->get_blockinvoke_level() == 0) {
			//TODO : check invoke_event-> get_root must inside the group
			blockinvoke_ev_t * begin = dynamic_cast<blockinvoke_ev_t *>(invoke_event->get_root());
			if (begin != NULL && dynamic_cast<dequeue_ev_t *>(begin->get_root())) {
				*cur_group_ptr = NULL;
			}
		}
		return cur_group;
	}
}

bool Groups::voucher_manipulation(event_t *event)
{
	if (event->get_op().find("MACH_IPC_voucher_") != string::npos
		&& event->get_op() != "MACH_IPC_voucher_info")
		return true;
	return false;
}

/*general group dividing*/
void Groups::init_groups(int idx, list<event_t*> & tid_list)
{
	/* per list data init */
	uint64_t tid = (tid_list.front())->get_tid();
	pid_t pid = tid2pid(tid);

	if (pid == -1)
		cerr << "Error : missing pid, tid = " << hex << tid << endl;
	//assert(pid != -1);

	string proc_comm = tid2comm(tid);
	if (proc_comm.size() == 0) {
		cerr << "Error : missing proc_comm, pid = " << dec << pid << " tid = " << hex << tid << endl; 
		proc_comm = "proc_" + to_string(pid);
		tid_comm[tid] = pid_comm[pid] = proc_comm;
	}

	bool is_ground = proc_comm == LoadData::meta_data.host ? true : false;

	map<uint64_t, group_t*> ret_map;
	uint64_t gid_base = tid << GROUP_ID_BITS;
	group_t *cur_group = NULL;

	intr_ev_t *potential_root = NULL;
	backtrace_ev_t * backtrace_for_hook = NULL;
	voucher_ev_t * voucher_for_hook = NULL;
	list<event_t *>::iterator it;
	event_t * event = NULL;

	/* checking every event in the same thread */
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		event->set_ground(is_ground);
		event->set_pid(pid);

		if (voucher_manipulation(event))
			continue;

		if (cur_group == NULL) {
			cur_group = create_group((gid_base + ret_map.size()), NULL);
			ret_map[cur_group->get_group_id()] = cur_group;
		}

		/* record interrupt */
		intr_ev_t * intr_event = dynamic_cast<intr_ev_t *>(event);
		if (intr_event) {
			intr_ev_t * dup_intr = new intr_ev_t(intr_event);
			intr_event->add_dup(dup_intr);
			cur_group->add_to_container(dup_intr);
			potential_root = intr_event;
			continue;
		}

		/* record backtrace */
		backtrace_ev_t * backtrace_event = dynamic_cast<backtrace_ev_t *>(event);
		if (backtrace_event) {
			backtrace_for_hook = backtrace_event;
			continue;
		}

		/* record voucher_info */
		voucher_ev_t * voucher_event = dynamic_cast<voucher_ev_t *>(event);
		if (voucher_event) {
			voucher_for_hook = voucher_event;
			continue;
		}

		/* isolate time share maintainance_makerunnable
		 * if called in csw, following the wait event
	 	 * if called in quntum timer expiration, following timer/??? interrupt
	 	 * both are followd by mkrunnable, close group after mkrunnable
	 	 */  
		tsm_ev_t *tsm_event = dynamic_cast<tsm_ev_t *>(event);
		if (tsm_event) {
			tsm_event->save_gptr((void*)cur_group);
			cur_group = create_group((gid_base + ret_map.size()), tsm_event);
			ret_map[cur_group->get_group_id()] = cur_group;
			continue;
		}
		
		/* isolate intrrupt_makerunnable */
		mkrun_ev_t * mr_event = dynamic_cast<mkrun_ev_t *>(event);
		if (mr_event) {
			group_t *ret = add_mr_into_group(mr_event, &cur_group, potential_root);
			if (ret->get_group_id() == (uint64_t)-1) {
				ret->set_group_id(gid_base + ret_map.size());
				ret_map[ret->get_group_id()] = ret;
			}
			continue;
		}
		
		/* check if close a group */
		wait_ev_t * wait_event = dynamic_cast<wait_ev_t *>(event);
		if (wait_event) {
			add_wait_into_group(wait_event, &cur_group);
			continue;
		}

		/* in kernel thread, always has a new group
		 * because one callthread(kernel thread) might
		 * invoke multiple timer timercallouts
		 */
		timercallout_ev_t * timercallout_event = dynamic_cast<timercallout_ev_t*>(event);
		if (timercallout_event) {
			assert(cur_group->get_blockinvoke_level() == 0);
			cur_group = create_group((gid_base + ret_map.size()), NULL);
			ret_map[cur_group->get_group_id()] = cur_group;
			cur_group->add_to_container(timercallout_event);
			continue;
		} 

		/* hook backtrace to blockinvoke event*/
		blockinvoke_ev_t * invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
		if (invoke_event) {
			if (invoke_event->is_begin() == true
				&& backtrace_for_hook != NULL
				&& backtrace_for_hook->hook_to_event(event, BLOCKINVOKE)) {
				cur_group->add_to_container(backtrace_for_hook);
				cur_group->add_group_tags(backtrace_for_hook->get_symbols());
				backtrace_for_hook = NULL;
			}
			cur_group->add_group_tags(invoke_event->get_desc());
			add_invoke_into_group(invoke_event, &cur_group);
			continue;
		}

		enqueue_ev_t * enqueue_event = dynamic_cast<enqueue_ev_t *>(event);
		if (enqueue_event)
			enqueue_event->set_nested_level(cur_group->get_blockinvoke_level());

		dequeue_ev_t * dequeue_event = dynamic_cast<dequeue_ev_t *>(event);
		if (dequeue_event)
			dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());

		/* hook voucher / backtrace to msg event */
		msg_ev_t * msg_event = dynamic_cast<msg_ev_t*>(event);
		if (msg_event) {
			/* freed msg */
			if (msg_event->is_freed_before_deliver()) {
				if (voucher_for_hook && voucher_for_hook->hook_msg(msg_event))
					voucher_for_hook = NULL;
				if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, MSG_EVENT)) 
					backtrace_for_hook = NULL;
				/* TODO : Check if necessary to add mach msg not delivered before freed
				 * considering diffing
				 */
				cur_group->add_to_container(event);
				continue;
			}

			pid_t msg_peer = msg_event->get_peer() ? tid2pid(msg_event->get_peer()->get_tid()) : -1;
				
			/* hook voucher and make use of voucher to divide thread activity */
			if (voucher_for_hook && voucher_for_hook->hook_msg(msg_event)) {
				if (check_group_with_voucher(voucher_for_hook, cur_group, msg_peer) == false) {
					cur_group = create_group((gid_base + ret_map.size()), NULL);
					ret_map[cur_group->get_group_id()] = cur_group;
				}
				cur_group->set_group_msg_bank_holder(voucher_for_hook->get_bank_holder());
				cur_group->add_to_container(voucher_for_hook);
				voucher_for_hook = NULL;
			}
			/* msg without voucher, check with msg_peer in the group*/
			 else if (msg_event->get_procname() != LoadData::meta_data.host
					&& msg_event->get_header()->is_mig() == false
					&& cur_group->get_blockinvoke_level() == 0) {
				if (msg_peer != -1 && cur_group->get_group_msg_peer() != -1
					&& cur_group->get_group_msg_peer() != msg_peer) {
					cur_group = create_group((gid_base + ret_map.size()), NULL);
					ret_map[cur_group->get_group_id()] = cur_group;
				}
			}

			if (msg_event->get_header()->is_mig() == false && msg_peer != -1) {
				cur_group->set_group_msg_peer(msg_peer);
				cur_group->add_group_peer(pid2comm(msg_peer));
			}

			/* hook backtrace to mach msg*/
			if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, MSG_EVENT)) {
				cur_group->add_to_container(backtrace_for_hook);
				cur_group->add_group_tags(backtrace_for_hook->get_symbols());
				backtrace_for_hook = NULL;
			}
		}

		/* Rest cases */
		cur_group->add_to_container(event);
	}
	sub_results[idx] = ret_map;
}

/* false means a new group is needed */
bool Groups::check_group_with_voucher(voucher_ev_t * voucher_event, group_t * cur_group, pid_t msg_peer)
{
	uint32_t cur_group_nested_level = cur_group->get_blockinvoke_level();
	pid_t msg_bank_holder = voucher_event->get_bank_holder();
	pid_t cur_group_bank_holder = cur_group->get_group_msg_bank_holder();
	pid_t cur_group_msg_peer = cur_group->get_group_msg_peer();
	bool ret = false;
	
	if (voucher_event->get_procname() == LoadData::meta_data.host)
		return true;

	if (cur_group_nested_level != 0) {
		ret = true;
		goto out;
	}

	if (cur_group_bank_holder == -1) {
		// if you don't have voucher check msg_peer
		if (cur_group_msg_peer == -1 || msg_peer == -1 || cur_group_msg_peer == msg_peer) {
			ret = true;
		}
	} else {
		if (cur_group_bank_holder == msg_bank_holder)
			ret = true;
	}
	
out:
	return ret;
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
	groups.insert(sub_groups.begin(), sub_groups.end());
}

int Groups::decode_groups(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t*>::iterator it;

	output << "Total number of Groups = " << groups.size() << endl << endl;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t * cur_group = it->second;
		output << "\n\n#Group " << hex << cur_group->get_group_id() << "(length = " << hex << cur_group->get_size() <<"):\n";
		cur_group->decode_group(output);
	}
	output.close();
	return 0;
}

int Groups::streamout_groups(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t*>::iterator it;

	output << "Total number of Groups = " << groups.size() << endl << endl;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t * cur_group = it->second;
		output << "\n\n#Group " << hex << cur_group->get_group_id() << "(length = " << hex << cur_group->get_size() <<"):\n";
		cur_group->streamout_group(output);
	}
	output.close();
	return 0;
}

void Groups::para_group(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));

	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &ioService));
	
	tid_evlist_t::iterator it;
	int idx = 0;
	int main_idx = -1;

	for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
		if ((it->second).size() == 0)
			continue;
		if (it->first != 0 && it->first == main_thread) {
			MainThreadDivider main_thread_divider(idx, sub_results, it->second);
			ioService.post(boost::bind(&MainThreadDivider::divide, main_thread_divider));
			main_idx = idx;
		} else if (it->first != 0 && it->first == nsevent_thread) {
			NSEventThreadDivider nsevent_thread_divider(idx, sub_results, it->second);
			ioService.post(boost::bind(&NSEventThreadDivider::divide, nsevent_thread_divider));
		} else {
			ioService.post(boost::bind(&Groups::init_groups, this, idx, it->second));
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
	main_groups = sub_results[main_idx];
}
