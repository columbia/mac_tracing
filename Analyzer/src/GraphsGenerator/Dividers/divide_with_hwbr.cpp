#include "thread_divider.hpp"

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
}

