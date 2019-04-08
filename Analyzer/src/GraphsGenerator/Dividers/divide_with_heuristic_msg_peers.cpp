#include "thread_divider.hpp"
//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////
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

		if (cur_group->get_group_msg_peer() != -1
			&& cur_group->get_group_msg_peer() != msg_peer) {
			cur_group = NULL;
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
