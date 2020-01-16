#include "thread_divider.hpp"
//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////
void ThreadDivider::check_hooks(MsgEvent *msg_event)
{
    if (voucher_for_hook
            && voucher_for_hook->hook_msg(msg_event)) {
        voucher_for_hook = nullptr;
    }

    if (backtrace_for_hook
            && backtrace_for_hook->hook_to_event(msg_event, MSG_EVENT)) {
        backtrace_for_hook = nullptr;
    }
}

std::set<std::string> ThreadDivider::calculate_msg_peer_set(MsgEvent *msg_event,
VoucherEvent *voucher_event) {
    std::set<std::string> peer_set;

    if (msg_event->get_peer() != nullptr) {
        std::string peer_proc = msg_event->get_peer()->get_procname();
        if (peer_proc != msg_event->get_procname())
            peer_set.insert(peer_proc + std::to_string(msg_event->get_peer()->get_pid()));
    }
    
    if (voucher_event != nullptr) {
        std::string msg_bank_holder = voucher_event->get_bank_holder_name();
        std::string msg_bank_orig = voucher_event->get_bank_orig_name();
        std::string msg_bank_merchant = voucher_event->get_bank_merchant_name();

        pid_t msg_bank_holder_pid = voucher_event->get_bank_holder();
        pid_t msg_bank_orig_pid = voucher_event->get_bank_orig();
        pid_t msg_bank_merchant_pid = voucher_event->get_bank_merchant();

        if (msg_bank_holder_pid != -1 && msg_bank_holder != msg_event->get_procname()) {
            assert(msg_bank_holder == LoadData::pid2comm(msg_bank_holder_pid));
            peer_set.insert(msg_bank_holder + std::to_string(msg_bank_holder_pid));
        }

        if (msg_bank_merchant_pid != -1 && msg_bank_merchant != msg_event->get_procname())
            peer_set.insert(msg_bank_merchant + std::to_string(msg_bank_merchant_pid));

        if (msg_bank_orig_pid != -1 && msg_bank_orig != msg_event->get_procname()) {
            assert(msg_bank_orig == LoadData::pid2comm(msg_bank_orig_pid));
            peer_set.insert(msg_bank_orig + std::to_string(msg_bank_orig_pid));
        }
    }
    return peer_set;
}

void ThreadDivider::add_events(MsgEvent *msg_event, bool need_calculate)
{
    add_general_event_to_group(msg_event);
    BacktraceEvent *backtrace_event = msg_event->get_bt();
    if (backtrace_event) {
        add_general_event_to_group(backtrace_event);
        for (int i = 0; i < backtrace_event->get_size(); i++)
            cur_group->add_group_tags(backtrace_event->get_sym(i));
    }

    VoucherEvent *voucher_event = msg_event->get_voucher();
    if (voucher_event != nullptr)
        add_general_event_to_group(voucher_event);

    if (need_calculate)
        cur_group->add_group_peer(calculate_msg_peer_set(msg_event, voucher_event));
}

void ThreadDivider::add_msg_event_into_group(EventBase *event)
{
    MsgEvent *msg_event = dynamic_cast<MsgEvent *>(event);
    bool induce_group = false;

    assert(msg_event);
    check_hooks(msg_event);
#if DEBUG_THREAD_DIVIDER
    mtx.lock();
    if (msg_event->get_peer() == nullptr)
        std::cerr << "Missing peer for message at " \
        << std::fixed << std::setprecision(1) \
        << msg_event->get_abstime() << std::endl;
    else if (msg_event->get_peer()->get_pid() == -1)
        std::cerr << "Missing peer pid for message at " \
        << std::fixed << std::setprecision(1) \
        << msg_event->get_abstime() << std::endl;
    mtx.unlock();
#endif

    /*1. freed msg or currrent group is already new*/
    if (msg_event->is_freed_before_deliver() 
        || msg_event->get_peer() == nullptr
        || !cur_group
#ifndef ANTIBATCH
        || cur_group->get_blockinvoke_level() > 0
#endif
        ){
        add_events(msg_event, true);
        return;
    }

    /*2. check if a new group needed use msg peer heuristics*/
    std::set<std::string> cur_peer_set = calculate_msg_peer_set(msg_event, msg_event->get_voucher());   
    std::set<std::string> group_peer_set = cur_group->get_group_peer();
    if (cur_peer_set.size() == 0 || group_peer_set.size() == 0) {
        add_events(msg_event, false);  
        cur_group->add_group_peer(cur_peer_set);
        return;
    }

    induce_group = true;
    std::set<std::string>::iterator it;
    for (it = cur_peer_set.begin(); it != cur_peer_set.end(); it++) {
        if (group_peer_set.find(*it) != group_peer_set.end())
            induce_group = false;
    }

    if (induce_group == true) {
        if (cur_group)
            cur_group->set_divide_by_msg(DivideNewGroup);
        Group *new_group = create_group(gid_base + ret_map.size(), nullptr);
        ret_map[new_group->get_group_id()] = new_group;
        new_group->set_blockinvoke_level(cur_group->get_blockinvoke_level());

        cur_group = new_group;
        msg_induced_node++;
        cur_group->set_divide_by_msg(DivideOldGroup);
    }

    add_events(msg_event, false);
    cur_group->add_group_peer(cur_peer_set);

    /* 3. hook mach syscall mach_msg_trap
    if (syscall_event
       && (syscall_event->get_op() == "MSC_mach_msg_trap"
           || syscall_event->get_op() == "MSC_mach_msg_overwrite_trap")
       && msg_event->get_user_addr() == syscall_event->get_arg(0))
       add_general_event_to_group(syscall_event);
    */
}
