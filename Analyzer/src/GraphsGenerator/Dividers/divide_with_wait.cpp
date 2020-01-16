#include "thread_divider.hpp"

//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////

bool ThreadDivider::matching_wait_syscall(WaitEvent *wait)
{
    SyscallEvent *syscall_event = nullptr;
    std::list<EventBase *> &ev_list = cur_group->get_container();
    int count =  ev_list.size() < 5 ? ev_list.size() : 5;

    std::list<EventBase *>::reverse_iterator rit;
    for (rit = ev_list.rbegin(); rit != ev_list.rend() && count > 0; rit++, count--) {
        if ((*rit)->get_event_type() != SYSCALL_EVENT)
            continue;
        syscall_event = dynamic_cast<SyscallEvent *>(*rit);
        if (wait->get_abstime() < syscall_event->get_ret_time()) {
            wait->pair_syscall(syscall_event);
            return true;
        } else
            break;
    }
    return false;
}
void ThreadDivider::add_wait_event_to_group(EventBase *event)
{
    add_general_event_to_group(event);
    
    if (event->get_procname() != "kernel_task") {
        WaitEvent *wait_event = dynamic_cast<WaitEvent *>(event);
        assert(wait_event);
        matching_wait_syscall(wait_event);
    }

#ifdef ANTIBATCH
    if (cur_group->get_blockinvoke_level() != 0) {
        cur_group->set_disp_divide_by_wait(DivideOldGroup);
        Group *new_group = create_group(gid_base + ret_map.size(), nullptr);
        ret_map[new_group->get_group_id()] = new_group;
        new_group->set_blockinvoke_level(cur_group->get_blockinvoke_level());

        cur_group = new_group;
        cur_group->set_disp_divide_by_wait(DivideNewGroup);
        wait_block_disp_node++;
    } 
#endif
    /* if not in the middle of dispatch block execution
     * close cur_group
     * otherwise make a whole dispatch block in one group
     */
    if (cur_group->get_blockinvoke_level() == 0) {
        cur_group->set_divide_by_wait(DivideOldGroup);
        cur_group = nullptr;
    }
}
