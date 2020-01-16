#include "thread_divider.hpp"
#define DEBUG_THREAD_DIVIDER 0

RunLoopThreadDivider::RunLoopThreadDivider(int _index,std::map<uint64_t,std::map<uint64_t, Group *> >&sub_results, std::list<EventBase *> tid_list, bool _no_entry_observer)
:ThreadDivider(_index, sub_results, tid_list)
{
    no_entry_observer = _no_entry_observer;
    save_cur_rl_group_for_invoke = nullptr;
    invoke_in_rl = nullptr;
}

RunLoopThreadDivider::~RunLoopThreadDivider(void)
{
}

void RunLoopThreadDivider::add_observer_event_to_group(EventBase *event)
{
    RunLoopObserverEvent *rl_observer_event = dynamic_cast<RunLoopObserverEvent *>(event);
    if (rl_observer_event->get_stage() == kCFRunLoopEntry) {
        cur_group = nullptr;
    }

    if (no_entry_observer == true && rl_observer_event->get_stage() == kCFRunLoopExtraEntry)
        cur_group = nullptr;

    add_general_event_to_group(event);
}

void RunLoopThreadDivider::add_disp_invoke_event_to_group(EventBase *event)
{
    /*processing the backtrace*/
    BlockInvokeEvent *invoke_event = dynamic_cast<BlockInvokeEvent *>(event);
    assert(invoke_event);

    if (invoke_event->is_begin()) {
        /* processing the backtrace */
        if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT)) {
            backtrace_for_hook = nullptr;
        }

        //if (save_cur_rl_group_for_invoke == nullptr) {
        if (invoke_in_rl == nullptr && invoke_event->get_root()) {
            save_cur_rl_group_for_invoke = cur_group;
            invoke_in_rl = invoke_event;
            cur_group = nullptr;
        }

        add_general_event_to_group(invoke_event);
        assert(cur_group);

        //if (invoke_event->get_bt()) {
            //assert(dynamic_cast<BacktraceEvent *>(invoke_event->get_bt()));
            //add_general_event_to_group(invoke_event->get_bt());
        //}
        if (invoke_event->get_root())
            add_general_event_to_group(invoke_event->get_root());
        assert(cur_group);
        invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
        cur_group->blockinvoke_level_inc();
    } else {
        if (!invoke_event->get_root() || !cur_group || cur_group->get_blockinvoke_level() <= 0) {
#if DEBUG_RunLoopThreadTypeEAD_DIVIDER
            mtx.lock();
            std::cerr <<"unbalanced block invoke pair at " << std::fixed << std::setprecision(1) << event->get_abstime() << std::endl;
            mtx.unlock();
#endif
            add_general_event_to_group(event);
            cur_group = save_cur_rl_group_for_invoke;
            save_cur_rl_group_for_invoke = nullptr;
            invoke_in_rl = nullptr;
            return;
        }

        assert(invoke_event->get_root());
        assert(cur_group);
        assert(cur_group->get_blockinvoke_level() > 0);
        add_general_event_to_group(event);
        cur_group->blockinvoke_level_dec();
        invoke_event->set_nested_level(cur_group->get_blockinvoke_level());

        if (cur_group->get_blockinvoke_level() == 0 && invoke_in_rl && invoke_event->get_root() == invoke_in_rl) {
                cur_group = save_cur_rl_group_for_invoke;
                save_cur_rl_group_for_invoke = nullptr;
                invoke_in_rl = nullptr;
        }
    }
}

void RunLoopThreadDivider::add_msg_event_into_group(EventBase *event)
{
    MsgEvent * msg_event = dynamic_cast<MsgEvent *>(event);
    if (voucher_for_hook
            && voucher_for_hook->hook_msg(msg_event)) {
        add_general_event_to_group(voucher_for_hook);
        voucher_for_hook = nullptr;
    }

    if (backtrace_for_hook
            && backtrace_for_hook->hook_to_event(event, MSG_EVENT)) {
        add_general_event_to_group(backtrace_for_hook);
        backtrace_for_hook = nullptr;
    }

    add_general_event_to_group(event);
}

void RunLoopThreadDivider::add_nsappevent_event_to_group(EventBase *event)
{
    //NSAppEventEvent *nsappevent = dynamic_cast<NSAppEventEvent *>(event);
    cur_group = nullptr;
    add_general_event_to_group(event);
}

void RunLoopThreadDivider::add_rlboundary_event_to_group(EventBase *event)
{
    RunLoopBoundaryEvent *boundary_event = dynamic_cast<RunLoopBoundaryEvent *>(event);
    assert(boundary_event);
    switch(boundary_event->get_state()) {
        case ItemBegin:
            cur_group = nullptr;
            add_general_event_to_group(event);
            break;
        case ItemEnd:
            add_general_event_to_group(event);
            cur_group = nullptr;
            break;
        case CategoryBegin:
        case CategoryEnd:    
        default:
            add_general_event_to_group(event);
            break;
    }
}

void RunLoopThreadDivider::divide()
{
    std::list<EventBase *>::iterator it;
    EventBase * event;
    for (it = tid_list.begin(); it != tid_list.end(); it++) {
        event = *it;
        switch (event->get_event_type()) {
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
            case FAKED_WOKEN_EVENT:
            //case DISP_DEQ_EVENT:
                store_event_to_group_handler(event);
                break;
           // case TSM_EVENT:
           //     add_tsm_event_to_group(event);
           //     break;
           // case MR_EVENT:
           //     add_mr_event_to_group(event);
           //     break;
            case WAIT_EVENT:
                /*
                add_general_event_to_group(event);
                if (event->get_procname() != "kernel_task")
                    matching_wait_syscall(dynamic_cast<WaitEvent *>(event));
                */
                add_wait_event_to_group(event);
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
            case DISP_DEQ_EVENT: {
                BlockDequeueEvent *dequeue_event = dynamic_cast<BlockDequeueEvent *>(event);
                if (dequeue_event->is_executed()) {
                    store_event_to_group_handler(event);
                } else {
                    add_general_event_to_group(event);
                    dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
                }
                break;
            }
            case RL_BOUNDARY_EVENT:
                add_rlboundary_event_to_group(event);
                break;
            default:
                add_general_event_to_group(event);
                break;
        }
    }
    submit_result[index] = ret_map;
}
