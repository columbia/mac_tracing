#include "thread_divider.hpp"

//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////

bool ThreadDivider::matching_wait_syscall(wait_ev_t *wait)
{
	syscall_ev_t *syscall_event = NULL;
	list<event_t *> &ev_list = cur_group->get_container();
	int count =  ev_list.size() < 5 ? ev_list.size() : 5;

	list<event_t *>::reverse_iterator rit;
	for (rit = ev_list.rbegin(); rit != ev_list.rend() && count > 0; rit++, count--) {
		if ((*rit)->get_event_id() != SYSCALL_EVENT)
			continue;
		syscall_event = dynamic_cast<syscall_ev_t *>(*rit);
		if (wait->get_abstime() < syscall_event->get_ret_time()) {
			wait->pair_syscall(syscall_event);
			return true;
		} else
			break;
	}
	return false;
}

void ThreadDivider::add_wait_event_to_group(event_t *event)
{
	add_general_event_to_group(event);
	
	if (event->get_procname() != "kernel_task") {
		wait_ev_t *wait_event = dynamic_cast<wait_ev_t *>(event);
		assert(wait_event);
		matching_wait_syscall(wait_event);
	}
	/* if not in the middle of dispatch block execution
	 * close cur_group
	 * otherwise make a whole dispatch block in one group
	 */
	if (cur_group->get_blockinvoke_level() == 0)
		cur_group = NULL;
}
