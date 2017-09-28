#include "thread_divider.hpp"

RLThreadDivider::RLThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> tid_list, bool _no_entry_observer)
:ThreadDivider(_index, sub_results, tid_list)
{
	no_entry_observer = _no_entry_observer;
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
	if (invoke_event->is_begin()
			&& backtrace_for_hook
			&& backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT)) {
		add_general_event_to_group(backtrace_for_hook);
		cur_group->add_group_tags(backtrace_for_hook->get_symbols());
		backtrace_for_hook = NULL;
	}
	add_general_event_to_group(event);
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

	if (msg_link
			&& msg_event->get_user_addr() == msg_link->get_arg(0))
		add_general_event_to_group(msg_link);

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
	string proc_comm = (tid_list.front())->get_procname();
	is_ground = proc_comm == LoadData::meta_data.host ? true : false;

	list<event_t *>::iterator it;
	event_t * event;
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		if (event->get_procname().size() == 0)
			event->override_procname(LoadData::meta_data.host);

		switch (event->get_event_id()) {
			case VOUCHER_CONN_EVENT:
			case VOUCHER_DEALLOC_EVENT:
			case VOUCHER_TRANS_EVENT:
				break;
			case SYSCALL_EVENT:
				if (event->get_op() != "MSC_mach_msg_trap"
						&& event->get_op() != "MSC_mach_msg_overwrite_trap") {
					add_general_event_to_group(event);
					break;
				}
			case INTR_EVENT:
				add_general_event_to_group(event);
			case BACKTRACE_EVENT:
			case VOUCHER_EVENT:
				store_event_to_group_handler(event);
				break;
			case MR_EVENT:
				add_mr_event_to_group(event);
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
