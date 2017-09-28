#include "thread_divider.hpp"

ThreadDivider::ThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list)
:submit_result(sub_results)
{
	gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
	cur_group = NULL;
	potential_root = NULL;
	backtrace_for_hook = NULL;
	voucher_for_hook = NULL;
	msg_link = NULL;
	pending_msg_sent = NULL;
	ret_map.clear();

	tid_list = ev_list;
	index = _index;
}

pid_t ThreadDivider::tid2pid(uint64_t tid)
{
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].first;
	}
	cerr << "No pid found for tid " << hex << tid << endl;
	return (pid_t)-1;
}

string ThreadDivider::tid2comm(uint64_t tid)
{
	string proc_comm = "";
	if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end()) {
		return LoadData::tpc_maps[tid].second;
	}
	cerr << "No comm found for tid " << hex << tid << endl;
	return proc_comm;
}

string ThreadDivider::pid2comm(pid_t pid)
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

group_t *ThreadDivider::create_group(uint64_t id, event_t *root_event)
{
	group_t *new_gptr = new group_t(id, root_event);
	if (root_event) 
		new_gptr->add_to_container(root_event);
	if (new_gptr == NULL)
		cerr << "Error : OOM can not create a new group." << endl;
	return new_gptr;
}

void ThreadDivider::delete_group(group_t *del_group)
{
	del_group->empty_container();
	delete(del_group);
}

void ThreadDivider::add_general_event_to_group(event_t *event)
{
	if (cur_group)
		cur_group->add_to_container(event);

 	else if (ret_map.find(gid_base + ret_map.size()) == ret_map.end()) {
		cur_group = create_group(gid_base + ret_map.size(), NULL);
		ret_map[cur_group->get_group_id()] = cur_group;
		cur_group->add_to_container(event);
	}

	else {
		cerr << "Error: fail to create a new group" << endl;
		exit(EXIT_FAILURE);
	}
}

void ThreadDivider::store_event_to_group_handler(event_t *event)
{
	switch (event->get_event_id()) {
		case INTR_EVENT:
			potential_root = dynamic_cast<intr_ev_t *>(event);
			break;
		case BACKTRACE_EVENT:
			backtrace_for_hook = dynamic_cast<backtrace_ev_t *>(event);
			break;
		case VOUCHER_EVENT:
			voucher_for_hook = dynamic_cast<voucher_ev_t *>(event);
			break;
		case SYSCALL_EVENT:
			msg_link = dynamic_cast<syscall_ev_t *>(event);
			break;
		case MSG_EVENT: {
			msg_ev_t *msg_event = dynamic_cast<msg_ev_t*>(event);
			if (msg_event->get_header()->check_recv() == false && msg_event->get_user_addr()) {
				pending_msg_sent = msg_event;
			}	
			break;
		}
		default:
			break;
	}
}

void ThreadDivider::add_tsm_event_to_group(event_t *event)
{
	/* isolate time share maintainance_makerunnable
	 * if called in csw, following the wait event
	 * if called in quntum timer expiration, following timer/??? interrupt
	 * both are followd by mkrunnable, close group after mkrunnable
	 */  
	tsm_ev_t *tsm_event = dynamic_cast<tsm_ev_t *>(event);
	tsm_event->save_gptr((void *)(cur_group));
	cur_group = create_group(gid_base + ret_map.size(), tsm_event);
	ret_map[cur_group->get_group_id()] = cur_group;
}

void ThreadDivider::add_mr_event_to_group(event_t *event)
{
	mkrun_ev_t *mr_event = dynamic_cast<mkrun_ev_t *>(event);
	event_t *last_event = cur_group? cur_group->get_last_event() : NULL;
	uint64_t mr_type = mr_event->check_mr_type(last_event, potential_root);
	switch (mr_type) {
		case SCHED_TSM_MR: {
			/* processing sched_timeshare_maintainance_mkrun
			 * [delayed] close the cur_group and reload stored group
			 */
			add_general_event_to_group(event);
			tsm_ev_t *tsm = dynamic_cast<tsm_ev_t *>(last_event);
			cur_group = ((group_t *)(tsm->load_gptr()));
			break;
		}
		case INTR_MR: { 
			/* 1. Intr and Mr events will be in a new group
			 * save and restore the previous active group
			 * 2. Do not touch potental_root
			 * one intr may triger multiple mkrunnable
			 * 3. Check the case that an active thread being waken up again
			 */
			group_t *saved_cur_group = cur_group;
			cur_group = create_group(gid_base + ret_map.size(), potential_root);
			ret_map[cur_group->get_group_id()] = cur_group;
			add_general_event_to_group(event);
			cur_group = saved_cur_group;
			break;
		}
		default:
			add_general_event_to_group(event);
			break;
	}
}

void ThreadDivider::add_wait_event_to_group(event_t *event)
{
	add_general_event_to_group(event);
	/* if not in the middle of dispatch block execution
	 * close cur_group
	 * otherwise make a whole dispatch block in one group
	 */
	if (cur_group->get_blockinvoke_level() == 0)
		cur_group = NULL;
}

void ThreadDivider::add_timercallout_event_to_group(event_t *event)
{
	/* in kernel thread, always has a new group
	 * because one callthread(kernel thread) might
	 * invoke multiple timer timercallouts
	 */
	if (cur_group) {
		assert(cur_group->get_blockinvoke_level() == 0);
		cur_group = NULL;
	}
	add_general_event_to_group(event);
}

void ThreadDivider::add_disp_invoke_event_to_group(event_t *event)
{
	/*1. processing the backtrace */
	blockinvoke_ev_t *invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
	if (invoke_event->is_begin() && backtrace_for_hook) {
		if (backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT)){
			add_general_event_to_group(backtrace_for_hook);
			cur_group->add_group_tags(backtrace_for_hook->get_symbols());
			backtrace_for_hook = NULL;
		}
	}
	
	/*2. add invoke event into group */
	add_general_event_to_group(event);
	cur_group->add_group_tags(invoke_event->get_desc());

	if (invoke_event->is_begin()) {
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		cur_group->blockinvoke_level_inc();
	} else {
		assert(invoke_event->get_root());
		if (cur_group->get_blockinvoke_level() <= 0) {
			cerr << "unbalanced invoke at " << fixed << setprecision(1) \
				<< invoke_event->get_abstime() << endl;
			assert(cur_group->get_first_event());
			cerr << "group begins at " << fixed << setprecision(1) \
				<< cur_group->get_first_event()->get_abstime() << endl;
		}
		assert(cur_group->get_blockinvoke_level() > 0);

		cur_group->blockinvoke_level_dec();
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());

		if (cur_group->get_blockinvoke_level() == 0) { // end of invoke
			//TODO : check corresponding dequeue event (invoke_event->get_root) is in the group
			blockinvoke_ev_t *begin = dynamic_cast<blockinvoke_ev_t *>(invoke_event->get_root());
			if (begin && dynamic_cast<dequeue_ev_t *>(begin->get_root()))
				cur_group = NULL;
		}
	}
}

/* false means a new group is needed */
bool ThreadDivider::check_group_with_voucher(voucher_ev_t *voucher_event,
	group_t *cur_group, pid_t msg_peer)
{
	uint32_t cur_group_nested_level = cur_group->get_blockinvoke_level();
	pid_t cur_group_bank_holder = cur_group->get_group_msg_bank_holder();
	pid_t cur_group_msg_peer = cur_group->get_group_msg_peer();

	pid_t msg_bank_holder = voucher_event->get_bank_holder();
	pid_t msg_bank_orig = voucher_event->get_bank_orig();

	bool ret = false;
	
	if (cur_group_nested_level != 0) {
		cerr << "0. Connection introduced by block invoke at ";
		cerr << fixed << setprecision(1) << voucher_event->get_abstime() << endl; 
		return true;
	}

	if (cur_group_bank_holder == -1) {
		// first voucher appears in the group
		if (cur_group_msg_peer == -1
			|| msg_peer == -1
			|| cur_group_msg_peer == msg_peer
			|| cur_group_msg_peer == msg_bank_holder
			|| cur_group_msg_peer == msg_bank_orig) {
			cerr << "1. Connection introduced by voucher at ";
			cerr << fixed << setprecision(1) << voucher_event->get_abstime() << endl; 
			ret = true;
		}
	} else {
		// voucher appears in the group
		if (cur_group_bank_holder == msg_bank_holder
			|| cur_group_bank_holder == msg_bank_orig) {
			ret = true;
			cerr << "2. Connection introduced by voucher at ";
			cerr << fixed << setprecision(1) << voucher_event->get_abstime() << endl; 
		}
	}

	return ret;
}

void ThreadDivider::add_msg_event_into_group(event_t *event)
{
	msg_ev_t * msg_event = dynamic_cast<msg_ev_t *>(event);

	/*1. freed msg */
	if (msg_event->is_freed_before_deliver()) {
		if (voucher_for_hook
			&& voucher_for_hook->hook_msg(msg_event)) {
			add_general_event_to_group(voucher_for_hook);
			voucher_for_hook = NULL;
		}
		if (backtrace_for_hook
			&& backtrace_for_hook->hook_to_event(event, MSG_EVENT)) {
			add_general_event_to_group(backtrace_for_hook);
			backtrace_for_hook = NULL;
		}
		if (msg_link && msg_event->get_user_addr() == msg_link->get_arg(0))
			add_general_event_to_group(msg_link);
		add_general_event_to_group(event);
		return;
	}

	/*2. check if a new group needed*/
	pid_t msg_peer = msg_event->get_peer() ? tid2pid(msg_event->get_peer()->get_tid()) : -1;
	/* 2.1. hook voucher and make use of voucher to divide thread activity */
	if (cur_group && voucher_for_hook
		&& voucher_for_hook->hook_msg(msg_event)
		&& voucher_for_hook->get_bank_holder() != msg_event->get_pid()) {
		/*if need a new group*/
		if (check_group_with_voucher(voucher_for_hook, cur_group, msg_peer) == false)
			cur_group = NULL;

		add_general_event_to_group(voucher_for_hook);
		cur_group->set_group_msg_bank_holder(voucher_for_hook->get_bank_holder());
		voucher_for_hook = NULL;
	}
	/* 2.2. for the msg without voucher, check with msg_peer in the group*/
	else if (msg_event->get_procname() != LoadData::meta_data.host
			&& msg_event->get_header()->is_mig() == false
			&& cur_group
			&& cur_group->get_blockinvoke_level() == 0) {
		/*if need a new group*/
		if (msg_peer != -1
			&& cur_group->get_group_msg_peer() != -1
			&& cur_group->get_group_msg_peer() != msg_peer)
			cur_group = NULL;
	}

	add_general_event_to_group(msg_event);
	if (msg_event->get_header()->is_mig() == false && msg_peer != -1) {
		cur_group->set_group_msg_peer(msg_peer);
		cur_group->add_group_peer(pid2comm(msg_peer));
	}

	/* 3. hook backtrace to mach msg*/
	if (backtrace_for_hook
		&& backtrace_for_hook->hook_to_event(event, MSG_EVENT)) {
		add_general_event_to_group(backtrace_for_hook);
		cur_group->add_group_tags(backtrace_for_hook->get_symbols());
		backtrace_for_hook = NULL;
	}
	
	/* 4. hook mach syscall mach_msg_trap
	if (msg_link && msg_event->get_user_addr() == msg_link->get_arg(0))
		add_general_event_to_group(msg_link);
	*/
}

void ThreadDivider::add_hwbr_event_into_group(event_t *event)
{
	breakpoint_trap_ev_t *hwtrap_event = dynamic_cast<breakpoint_trap_ev_t *>(event);
	assert(hwtrap_event && event->get_procname() == "WindowServer");
	if (hwtrap_event->check_read() == true
			&& pending_msg_sent && pending_msg_sent->get_group_id() >= gid_base) {
		//TODO : nearest msg send with MSC_mach_msg_overwrite_trap
		group_t *save_cur_group = cur_group;
		cur_group = ret_map[pending_msg_sent->get_group_id()];
		add_general_event_to_group(event);
		cur_group = save_cur_group;
		pending_msg_sent = NULL;
	} else {
		add_general_event_to_group(event);
	}

	/*
	if (msg_link && msg_link->get_group_id() != -1 && msg_link->get_group_id() >= gid_base) {
		group_t *save_cur_group = cur_group;
		cur_group = ret_map[msg_link->get_group_id()];
		add_general_event_to_group(event);
		cur_group = save_cur_group;
	} else {
		add_general_event_to_group(event);
		cur_group = NULL; //check if it is reasonable
	}
	*/

}

/* general group generation per thread */
void ThreadDivider::divide()
{
	/* per list data init */
	uint64_t tid = (tid_list.front())->get_tid();
	pid_t pid = tid2pid(tid);
	if (pid == -1)
		cerr << "Error : missing pid, tid = " << hex << tid << endl;

	string proc_comm = (tid_list.front())->get_procname();
	if (proc_comm.size() == 0) {
		cerr << "Error : missing proc_comm, pid = " << dec << pid\
			<< " tid = " << hex << tid << endl; 
		proc_comm = "proc_" + to_string(pid);
	}
	
	/* checking all events in the same thread */
	list<event_t *>::iterator it;
	event_t *event = NULL;
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		event->set_ground(is_ground);
		event->set_pid(pid);
		switch (event->get_event_id()) {
			case VOUCHER_CONN_EVENT:
			case VOUCHER_DEALLOC_EVENT:
			case VOUCHER_TRANS_EVENT:
				break;
			case SYSCALL_EVENT:
				if (event->get_op() != "MSC_mach_msg_trap" && event->get_op() != "MSC_mach_msg_overwrite_trap") {
					if (event->get_op() != "BSC_sigreturn")
						add_general_event_to_group(event);
					break;
				}
			case INTR_EVENT:
				add_general_event_to_group(event);
			case BACKTRACE_EVENT:
			case VOUCHER_EVENT:
				store_event_to_group_handler(event);
				break;
			case TSM_EVENT:
				add_tsm_event_to_group(event);
				break;
			case MR_EVENT:
				add_mr_event_to_group(event);
				break;
			case WAIT_EVENT:
				add_wait_event_to_group(event);
				break;
			case TMCALL_CALLOUT_EVENT:
				add_timercallout_event_to_group(event);
				break;
			case DISP_ENQ_EVENT: {
				add_general_event_to_group(event);
				enqueue_ev_t *enqueue_event = dynamic_cast<enqueue_ev_t *>(event);
				enqueue_event->set_nested_level(cur_group->get_blockinvoke_level());
				break;
			}
			case DISP_DEQ_EVENT: {
				add_general_event_to_group(event);
				dequeue_ev_t *dequeue_event = dynamic_cast<dequeue_ev_t *>(event);
				dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
				break;
			}
			case DISP_INV_EVENT:
				add_disp_invoke_event_to_group(event);
				break;
			case MSG_EVENT:
				add_msg_event_into_group(event);
				store_event_to_group_handler(event);
				break;
			
			case BREAKPOINT_TRAP_EVENT:
				add_hwbr_event_into_group(event);
				break;
		
			default:
				add_general_event_to_group(event);
		}
	}
	submit_result[index] = ret_map;
}

void ThreadDivider::decode_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->decode_group(output);
	}
	output.close();
}

void ThreadDivider::streamout_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->streamout_group(output);
	}
	output.close();
}

