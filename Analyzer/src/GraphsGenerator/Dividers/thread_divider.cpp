#include "thread_divider.hpp"

#define DEBUG_THREAD_DIVIDER 0
ThreadDivider::ThreadDivider(int _index,std::map<uint64_t,std::map<uint64_t, Group *> >&sub_results, std::list<EventBase *> ev_list)
:submit_result(sub_results)
{
    gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
    cur_group = nullptr;
    potential_root = nullptr;
    backtrace_for_hook = nullptr;
    voucher_for_hook = nullptr;
    syscall_event = nullptr;
    pending_msg_sent = nullptr;
    dequeue_event = nullptr;
    faked_wake_event = nullptr;
    msg_induced_node = wait_block_disp_node= 0;
    
    ret_map.clear();
    tid_list = ev_list;
    index = _index;
}

Group *ThreadDivider::create_group(uint64_t id, EventBase *root_event)
{
    Group *new_gptr = new Group(id, root_event);

    if (new_gptr == nullptr) {
#if DEBUG_THREAD_DIVIDER
        mtx.lock();
        std::cerr << "Error: OOM can not create a new group." << std::endl;
        mtx.unlock();
#endif
        exit(EXIT_FAILURE);
    }

    if (root_event) 
        new_gptr->add_to_container(root_event);
    return new_gptr;
}

Group *ThreadDivider::gid2group(uint64_t gid)
{
    if (ret_map.find(gid) != ret_map.end())
        return ret_map[gid];
    return nullptr;
}

void ThreadDivider::delete_group(Group *del_group)
{
    del_group->empty_container();
    delete(del_group);
}

void ThreadDivider::add_general_event_to_group(EventBase *event)
{
    if (!cur_group) {
        cur_group = create_group(gid_base + ret_map.size(), nullptr);
        ret_map[cur_group->get_group_id()] = cur_group;
    }

    if (faked_wake_event) {
        cur_group->add_to_container(faked_wake_event);
        faked_wake_event = nullptr;
    }
    cur_group->add_to_container(event);
}

void ThreadDivider::store_event_to_group_handler(EventBase *event)
{
    switch (event->get_event_type()) {
        case INTR_EVENT:
            potential_root = dynamic_cast<IntrEvent *>(event);
            break;
        case BACKTRACE_EVENT:
            backtrace_for_hook = dynamic_cast<BacktraceEvent *>(event);
            assert(backtrace_for_hook->get_op() ==  "ARGUS_DBG_BT");
            break;
        case VOUCHER_EVENT:
            voucher_for_hook = dynamic_cast<VoucherEvent *>(event);
            break;
        case SYSCALL_EVENT:
            syscall_event = dynamic_cast<SyscallEvent *>(event);
            if (event->get_op() == "MSC_mach_msg_overwrite_trap")
                pending_msg_sent = syscall_event;
            break;
        case DISP_DEQ_EVENT:
            dequeue_event = dynamic_cast<BlockDequeueEvent *>(event);
            break;
        case FAKED_WOKEN_EVENT:
            faked_wake_event = dynamic_cast<FakedWokenEvent *>(event);
            break;
        default:
            break;
    }
}

/* general group generation per thread */
void ThreadDivider::divide()
{
    std::list<EventBase *>::iterator it;
    EventBase *event = nullptr;

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
            //case TSM_EVENT:
            //    add_tsm_event_to_group(event);
            //    break;
            //case MR_EVENT:
            //    add_mr_event_to_group(event);
            //    break;
            case WAIT_EVENT:
                add_wait_event_to_group(event);
                break;
            case TMCALL_CALLOUT_EVENT:
                add_timercallout_event_to_group(event);
                break;
            case DISP_ENQ_EVENT: {
                add_general_event_to_group(event);
                BlockEnqueueEvent *enqueue_event = dynamic_cast<BlockEnqueueEvent *>(event);
                enqueue_event->set_nested_level(cur_group->get_blockinvoke_level());
                break;
            }
            case DISP_DEQ_EVENT: {
                BlockDequeueEvent *dequeue_event = dynamic_cast<BlockDequeueEvent *>(event);
                
                if (dequeue_event->is_executed()) {
                    store_event_to_group_handler(event);
                } else {
                    add_general_event_to_group(event);
                    dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
                }
                
                //add_general_event_to_group(event);
                //dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
                break;
            }
            case DISP_INV_EVENT:
                add_disp_invoke_event_to_group(event);
                break;
            case DISP_MIG_EVENT: {
                add_disp_mig_event_to_group(event);
                break;
            }
            case MSG_EVENT:
                add_msg_event_into_group(event);
                //store_event_to_group_handler(event);
                break;
            case BREAKPOINT_TRAP_EVENT:
                add_hwbr_event_into_group(event);
                break;
            default:
                add_general_event_to_group(event);
        }
    }
    submit_result[index] = ret_map;

#if DEBUG_THREAD_DIVIDER
    if (msg_induced_node || wait_block_disp_node) {
        tid_t tid = (*(tid_list.begin()))->get_tid();
        std::string proc_name = (*(tid_list.begin()))->get_procname();
        mtx.lock();
        std::cerr << "Thread Divider " << proc_name << " ";
        std::cerr << std::hex << index << " tid = 0x" << std::hex << tid << ":\n";
        std::cerr << "\tmsg_induced_node " << std::dec << msg_induced_node << std::endl;
        std::cerr << "\twait_block_disp_node " << std::dec << wait_block_disp_node<< std::endl;
        mtx.unlock();
    }
#endif
}

void ThreadDivider::decode_groups(std::map<uint64_t, Group *> & uievent_groups, std::string filepath)
{
    std::ofstream output(filepath);
    if (output.fail()) {
        mtx.lock();
        std::cerr << "Error: unable to open file " << filepath << " for write " << std::endl;
        mtx.unlock();
        return;
    }
    std::map<uint64_t, Group *>::iterator it;
    Group * cur_group;
    uint64_t index = 0;
    for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
        output << "#Group " << std::hex << index << std::endl;
        index++;
        cur_group = it->second;
        cur_group->decode_group(output);
    }
    output.close();
}

void ThreadDivider::streamout_groups(std::map<uint64_t, Group *> & uievent_groups, std::string filepath)
{
    std::ofstream output(filepath);
    if (output.fail()) {
        mtx.lock();
        std::cerr << "Error: unable to open file " << filepath << " for write " << std::endl;
        mtx.unlock();
        return;
    }
    std::map<uint64_t, Group *>::iterator it;
    Group * cur_group;
    uint64_t index = 0;
    for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
        output << "#Group " << std::hex << index << std::endl;
        index++;
        cur_group = it->second;
        cur_group->streamout_group(output);
    }
    output.close();
}
