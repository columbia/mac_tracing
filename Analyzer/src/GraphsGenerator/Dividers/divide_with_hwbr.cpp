#include "thread_divider.hpp"

void ThreadDivider::add_hwbr_event_into_group(EventBase *event)
{
    BreakpointTrapEvent *hwtrap_event = dynamic_cast<BreakpointTrapEvent *>(event);
    if (//hwtrap_event->get_trigger_var() == "gOutMsgPending"
        event->get_procname() == "WindowServer"
        && hwtrap_event->check_read() && pending_msg_sent) {
        //nearest msg send with MSC_mach_msg_overwrite_trap
#if 0 //DEBUG_THREAD_DIVIDER
        mtx.lock();
        std::cerr << "Add msg pending event at " << std::fixed << std::setprecision(1) << hwtrap_event->get_abstime();
        std::cerr << " to group with " << std::fixed << std::setprecision(1) << pending_msg_sent->get_abstime() << std::endl;
        std::cerr << " Event val " << hwtrap_event->get_trigger_var() << " = " << hwtrap_event->get_trigger_val() << std::endl;  
        mtx.unlock();
#endif
        Group *save_cur_group = cur_group;
        cur_group = ret_map[pending_msg_sent->get_group_id()];
        add_general_event_to_group(event);
        cur_group = save_cur_group;
        pending_msg_sent = nullptr;
        return;
    } 
    add_general_event_to_group(event);
}

