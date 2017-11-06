#include "thread_divider.hpp"
#define DEBUG_RLTHREAD_DIVIDER 1

RLThreadDivider::RLThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> tid_list, bool _no_entry_observer)
:ThreadDivider(_index, sub_results, tid_list)
{
	no_entry_observer = _no_entry_observer;
	save_cur_rl_group_for_invoke = NULL;
	invoke_in_rl = NULL;
}

RLThreadDivider::~RLThreadDivider(void)
{
}

void RLThreadDivider::add_observer_event_to_group(event_t *event)
{
	rl_observer_ev_t *rl_observer_event = dynamic_cast<rl_observer_ev_t *>(event);
	if (rl_observer_event->get_stage() == kCFRunLoopEntry) {
		cur_group = NULL;
	}

	if (no_entry_observer == true && rl_observer_event->get_stage() == kCFRunLoopExtraEntry)
		cur_group = NULL;

	add_general_event_to_group(event);
}

void RLThreadDivider::add_disp_invoke_event_to_group(event_t *event)
{
	/*processing the backtrace*/
	blockinvoke_ev_t *invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
	assert(invoke_event);

	if (invoke_event->is_begin()) {
		if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT))
			backtrace_for_hook = NULL;

		//if (save_cur_rl_group_for_invoke == NULL) {
		if (invoke_in_rl == NULL) {
			save_cur_rl_group_for_invoke = cur_group;
			invoke_in_rl = invoke_event;
			cur_group = NULL;
		}
		add_general_event_to_group(invoke_event);
		if (invoke_event->get_bt())
			add_general_event_to_group(invoke_event->get_bt());
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		cur_group->blockinvoke_level_inc();
	} else {
		assert(invoke_event->get_root());
#if DEBUG_RLTHREAD_DIVIDER
		if (cur_group->get_blockinvoke_level() <= 0) {
			mtx.lock();
			cerr <<"unbalanced block invoke pair at " << fixed << setprecision(1) << event->get_abstime() << endl;
			mtx.unlock();
			add_general_event_to_group(event);
			cur_group = save_cur_rl_group_for_invoke;
			save_cur_rl_group_for_invoke = NULL;
			invoke_in_rl = NULL;
			return;
		}
#endif
		assert(cur_group && cur_group->get_blockinvoke_level() > 0);
		add_general_event_to_group(event);
		cur_group->blockinvoke_level_dec();
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());

		if (cur_group->get_blockinvoke_level() == 0) {
			if (invoke_in_rl && invoke_event->get_root() == invoke_in_rl) {
				cur_group = save_cur_rl_group_for_invoke;
				save_cur_rl_group_for_invoke = NULL;
				invoke_in_rl = NULL;
			} 
		}
	}
}

void RLThreadDivider::add_msg_event_into_group(event_t *event)
{
	msg_ev_t * msg_event = dynamic_cast<msg_ev_t *>(event);
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
}

void RLThreadDivider::add_nsappevent_event_to_group(event_t *event)
{
	nsapp_event_ev_t *nsappevent = dynamic_cast<nsapp_event_ev_t *>(event);
	if (nsappevent->is_begin())
		cur_group = NULL;
	add_general_event_to_group(event);
}

void RLThreadDivider::divide()
{
	list<event_t *>::iterator it;
	event_t * event;
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
				if (event->get_procname() != "kernel_task")
					matching_wait_syscall(dynamic_cast<wait_ev_t *>(event));
				add_general_event_to_group(event);
				break;
			case DISP_INV_EVENT:
				add_disp_invoke_event_to_group(event);
				break;
			case MSG_EVENT:
				add_msg_event_into_group(event);
				break;
			case RL_OBSERVER_EVENT:
				add_observer_event_to_group(event);
				break;
			case NSAPPEVENT_EVENT:
				add_nsappevent_event_to_group(event);
				break;
			default:
				add_general_event_to_group(event);
				break;
		}
	}
	submit_result[index] = ret_map;
}
