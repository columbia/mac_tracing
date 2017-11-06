#include "thread_divider.hpp"
#define DEBUG_THREAD_DIVIDER 1

ThreadDivider::ThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list)
:submit_result(sub_results)
{
	gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
	cur_group = NULL;
	potential_root = NULL;
	backtrace_for_hook = NULL;
	voucher_for_hook = NULL;
	syscall_event = NULL;
	pending_msg_sent = NULL;
	dispatch_mig_server = NULL;
	mig_server_invoker = NULL;
	ret_map.clear();
	tid_list = ev_list;
	index = _index;
}

group_t *ThreadDivider::create_group(uint64_t id, event_t *root_event)
{
	group_t *new_gptr = new group_t(id, root_event);

	if (new_gptr == NULL) {
#if DEBUG_THREAD_DIVIDER
		mtx.lock();
		cerr << "Error: OOM can not create a new group." << endl;
		mtx.unlock();
#endif
		exit(EXIT_FAILURE);
	}

	if (root_event) 
		new_gptr->add_to_container(root_event);
	return new_gptr;
}

void ThreadDivider::delete_group(group_t *del_group)
{
	del_group->empty_container();
	delete(del_group);
}

void ThreadDivider::add_general_event_to_group(event_t *event)
{
	if (!cur_group) {
		cur_group = create_group(gid_base + ret_map.size(), NULL);
		ret_map[cur_group->get_group_id()] = cur_group;
	}
	cur_group->add_to_container(event);
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
			syscall_event = dynamic_cast<syscall_ev_t *>(event);
			if (event->get_op() == "MSC_mach_msg_overwrite_trap")
				pending_msg_sent = syscall_event;
			break;
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
	event_t *last_event = cur_group ? cur_group->get_last_event() : NULL;
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

bool ThreadDivider::matching_wait_syscall(wait_ev_t *wait)
{
	syscall_ev_t *syscall_event = NULL;
	list<event_t *> ev_list = cur_group->get_container();
	list<event_t *>::reverse_iterator rit;
	for (rit = ev_list.rbegin(); rit != ev_list.rend(); rit++) {
		if ((*rit)->get_event_id() != SYSCALL_EVENT)
			continue;
		syscall_event = dynamic_cast<syscall_ev_t *>(*rit);
		if (wait->get_abstime() < syscall_event->get_ret_time()) {
			wait->pair_syscall(syscall_event);
			return true;
		}
	}
	return false;
}

void ThreadDivider::add_wait_event_to_group(event_t *event)
{
	add_general_event_to_group(event);
	
	if (event->get_procname() != "kernel_task") {
		wait_ev_t *wait_event = dynamic_cast<wait_ev_t *>(event);
		matching_wait_syscall(wait_event);
	}
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
	blockinvoke_ev_t *invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
	assert(invoke_event);
	/*sanity check for block invoke event and restore group if dispatch_mig_server invoked*/
	if (invoke_event->is_begin()) {
		/* processing the backtrace */
		if (invoke_event->is_begin() && backtrace_for_hook
			&& backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT)){
				add_general_event_to_group(backtrace_for_hook);
				cur_group->add_group_tags(backtrace_for_hook->get_symbols());
				backtrace_for_hook = NULL;
		}
		add_general_event_to_group(event);
		cur_group->add_group_tags(invoke_event->get_desc());
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		cur_group->blockinvoke_level_inc();
	} else {
		assert(invoke_event->get_root());
		if (dispatch_mig_server && mig_server_invoker == invoke_event->get_root()) {
			cur_group = (group_t *)(dispatch_mig_server->restore_owner());
			assert(cur_group && cur_group->get_blockinvoke_level() > 0);
			dispatch_mig_server = NULL;
			mig_server_invoker = NULL;
		}
		add_general_event_to_group(event);
		cur_group->add_group_tags(invoke_event->get_desc());

#if DEBUG_THREAD_DIVIDER
		if (cur_group->get_blockinvoke_level() <= 0) {
			mtx.lock();
			cerr << "Error: unbalanced invoke at " << fixed << setprecision(1) << invoke_event->get_abstime() << endl;
			assert(cur_group->get_first_event());
			cerr << "group begins at " << fixed << setprecision(1) << cur_group->get_first_event()->get_abstime() << endl;
			mtx.unlock();
			cur_group = NULL;
			return;
		}
#endif
		assert(cur_group && cur_group->get_blockinvoke_level() > 0);
		cur_group->blockinvoke_level_dec();
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		if (cur_group->get_blockinvoke_level() == 0)
			cur_group = NULL;
	}
}

/* false means a new group is needed */
bool ThreadDivider::check_group_with_voucher(voucher_ev_t *voucher_event,
	group_t *cur_group, pid_t msg_peer)
{
	pid_t cur_group_bank_holder = cur_group->get_group_msg_bank_holder();
	pid_t cur_group_msg_peer = cur_group->get_group_msg_peer();
	pid_t msg_bank_holder = voucher_event->get_bank_holder();
	pid_t msg_bank_orig = voucher_event->get_bank_orig();

	bool ret = false;

	if (cur_group_bank_holder == -1) {
		//this is the first voucher in the group
		if (cur_group_msg_peer == -1
			//|| msg_peer == -1
			|| cur_group_msg_peer == msg_peer
			|| cur_group_msg_peer == msg_bank_holder
			|| cur_group_msg_peer == msg_bank_orig) {
#if 0 //DEBUG_THREAD_DIVIDER
			mtx.lock();
			cerr << "1. Connection introduced by voucher at ";
			cerr << fixed << setprecision(1) << voucher_event->get_abstime() << endl; 
			mtx.unlock();
#endif
			ret = true;
		}
	} else if (cur_group_bank_holder == msg_bank_holder
			|| cur_group_bank_holder == msg_bank_orig) {
#if 0 // DEBUG_THREAD_DIVIDER
			mtx.lock();
			cerr << "2. Connection introduced by voucher at ";
			cerr << fixed << setprecision(1) << voucher_event->get_abstime() << endl; 
			mtx.unlock();
#endif
			ret = true;
	}

	return ret;
}

void ThreadDivider::add_msg_event_into_group(event_t *event)
{
	msg_ev_t * msg_event = dynamic_cast<msg_ev_t *>(event);

	/*1. freed msg or currrent group is already new or cur group is inside dispatch block invoke*/
	if (msg_event->is_freed_before_deliver() || !cur_group || cur_group->get_blockinvoke_level()) {
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
		/*
		if (syscall_event
			&& (syscall_event->get_op() == "MSC_mach_msg_trap"
				|| syscall_event->get_op() == "MSC_mach_msg_overwrite_trap")
				&& msg_event->get_user_addr() == syscall_event->get_arg(0))
			add_general_event_to_group(syscall_event);
		*/
		add_general_event_to_group(event);
		return;
	}

	/*2. check if a new group needed*/
	pid_t msg_peer = msg_event->get_peer() ? Groups::tid2pid(msg_event->get_peer()->get_tid()) : -1;
#if DEBUG_THREAD_DIVIDER
	mtx.lock();
	if (msg_event->get_peer() == NULL)
		cerr << "Missing peer for message at " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
	else if (msg_peer == -1)
		cerr << "Missing peer pid for message at " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
	mtx.unlock();
#endif
	/* 2.1. hook voucher and make use of voucher, if not self, to divide thread activity */
	if (voucher_for_hook && voucher_for_hook->hook_msg(msg_event) 
		&& (voucher_for_hook->get_bank_holder() != msg_event->get_pid())) {
		/*if need a new group*/
		if (!check_group_with_voucher(voucher_for_hook, cur_group, msg_peer))
			cur_group = NULL;

		add_general_event_to_group(voucher_for_hook);
		cur_group->set_group_msg_bank_holder(voucher_for_hook->get_bank_holder());
		voucher_for_hook = NULL;
	}
	/* 2.2. for the msg without voucher or the voucher is from itself, check with msg_peer in the group*/
	else if (msg_peer != -1
			&& msg_event->get_procname() != LoadData::meta_data.host
			&& msg_event->get_header()->is_mig() == false) {
		/*if need a new group*/
		mtx.lock();
		if (msg_event->get_abstime() - 107799347286.4 > -10e-8 && msg_event->get_abstime() - 107799347286.4 < 10e-8)
			cerr << "Check mesage at 107799347286.4 " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
		mtx.unlock();

		if (cur_group->get_group_msg_peer() != -1
			&& cur_group->get_group_msg_peer() != msg_peer) {
			mtx.lock();
			if (msg_event->get_abstime() - 107799347286.4 > -10e-8 && msg_event->get_abstime() - 107799347286.4 < 10e-8) {
				cerr << "Check mesage at 107799347286.4 " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
				cerr << "group peer " << hex << cur_group->get_group_msg_peer() << "cur msg peer " << hex << msg_peer << endl;
				cerr << "Need a new group" << endl;
			}
			mtx.unlock();
			cur_group = NULL;
		} else {
			mtx.lock();
			if (msg_event->get_abstime() - 107799347286.4 > -10e-8 && msg_event->get_abstime() - 107799347286.4 < 10e-8) {
				cerr << "Check mesage at 107799347286.4 " << fixed << setprecision(1) << msg_event->get_abstime() << endl;
				cerr << "group peer " << hex << cur_group->get_group_msg_peer() << "cur msg peer " << hex << msg_peer << endl;
				cerr << "Do not need a new group" << endl;
			}
			mtx.unlock();
		}
		
	}

	add_general_event_to_group(msg_event);

	if (msg_event->get_header()->is_mig() == false && msg_peer != -1) {
		cur_group->set_group_msg_peer(msg_peer);
		cur_group->add_group_peer(Groups::pid2comm(msg_peer));
	}

	/* 3. hook backtrace to mach msg*/
	if (backtrace_for_hook
		&& backtrace_for_hook->hook_to_event(event, MSG_EVENT)) {
		add_general_event_to_group(backtrace_for_hook);
		cur_group->add_group_tags(backtrace_for_hook->get_symbols());
		backtrace_for_hook = NULL;
	}
	
	/* 4. hook mach syscall mach_msg_trap
	if (syscall_event
	   && (syscall_event->get_op() == "MSC_mach_msg_trap"
		   || syscall_event->get_op() == "MSC_mach_msg_overwrite_trap")
	   && msg_event->get_user_addr() == syscall_event->get_arg(0))
	   add_general_event_to_group(syscall_event);
	*/
}

void ThreadDivider::add_hwbr_event_into_group(event_t *event)
{
	breakpoint_trap_ev_t *hwtrap_event = dynamic_cast<breakpoint_trap_ev_t *>(event);
	if (//hwtrap_event->get_trigger_var() == "gOutMsgPending"
		event->get_procname() == "WindowServer"
		&& hwtrap_event->check_read() && pending_msg_sent) {
		//nearest msg send with MSC_mach_msg_overwrite_trap
#if 0 //DEBUG_THREAD_DIVIDER
		mtx.lock();
		cerr << "Add msg pending event at " << fixed << setprecision(1) << hwtrap_event->get_abstime();
		cerr << " to group with " << fixed << setprecision(1) << pending_msg_sent->get_abstime() << endl;
		cerr << " Event val " << hwtrap_event->get_trigger_var() << " = " << hwtrap_event->get_trigger_val() << endl;  
		mtx.unlock();
#endif
		group_t *save_cur_group = cur_group;
		cur_group = ret_map[pending_msg_sent->get_group_id()];
		add_general_event_to_group(event);
		cur_group = save_cur_group;
		pending_msg_sent = NULL;
		return;
	} 
	
	add_general_event_to_group(event);
#if 0 //DEBUG_THREAD_DIVIDER
	//if (hwtrap_event->get_trigger_var() == "gOutMsgPending") {
	if (event->get_procname() == "WindowServer") {
		mtx.lock();
		cerr << "Add msg pending event at " << fixed << setprecision(1) << hwtrap_event->get_abstime();
		cerr << " To current group" << endl;	
		mtx.unlock();
	}
#endif
}

void ThreadDivider::add_disp_mig_event_to_group(event_t *event, list<event_t *>::reverse_iterator rit)
{
	assert(cur_group);
	while (rit != tid_list.rend()) {
		if ((*rit)->get_event_id() == INTR_EVENT)
			rit++;
		else if (Groups::interrupt_mkrun_pair(*rit, rit)) {
			rit++;
			if (rit == tid_list.rend()) {
				mtx.lock();
				cerr << "Error: dispatch_mig_server called without block invoke" << endl;
				cerr << "\tEvent at " << fixed << setprecision(1) << event->get_abstime() << endl;
				mtx.unlock();
				return;
			}
			rit++;
		} else {
			break;
		}
	}
	if (rit == tid_list.rend()) {
		mtx.lock();
		cerr << "Error: dispatch_mig_server called without block invoke" << endl;
		cerr << "\tEvent at " << fixed << setprecision(1) << event->get_abstime() << endl;
		mtx.unlock();
		return;
	}

	event_t *last_event = *rit;
	if (last_event->get_event_id() != DISP_INV_EVENT) {
		mtx.lock();
		cerr << "Error: dispatch_mig_server called outside block invoke" << endl;
		cerr << "\tEvent at " << fixed << setprecision(1) << event->get_abstime() << endl;
		mtx.unlock();
		return;
	}
	
	mig_server_invoker = dynamic_cast<blockinvoke_ev_t *>(last_event);
	add_general_event_to_group(event);
	dispatch_mig_server->save_owner(cur_group);
	cur_group = NULL;
}

/* general group generation per thread */
void ThreadDivider::divide()
{
	list<event_t *>::iterator it;
	event_t *event = NULL;
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		switch (event->get_event_id()) {
			case VOUCHER_CONN_EVENT:
			case VOUCHER_DEALLOC_EVENT:
			case VOUCHER_TRANS_EVENT:
				break;
			case SYSCALL_EVENT:
				if (event->get_op() == "BSC_sigreturn")
					break;
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
			case DISP_MIG_EVENT: {
				dispatch_mig_server = dynamic_cast<disp_mig_ev_t *>(event);
				list<event_t *>::reverse_iterator rit(it);
				add_disp_mig_event_to_group(event, rit);
				break;
			}
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
		mtx.lock();
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		mtx.unlock();
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
		mtx.lock();
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		mtx.unlock();
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
