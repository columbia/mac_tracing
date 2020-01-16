#include "search_engine.hpp"
#include "canonization.hpp"
#include <map>


#define DEBUG_SEARCH_ENGINE 1

BugSearcher::BugSearcher(clusters_t *cluster_gen)
{
    cluster_map = cluster_gen->get_clusters();
    groups_ptr = cluster_gen->get_groups_ptr();
    ceiling_event = nullptr;
    floor_event = nullptr;
    
}

BugSearcher::BugSearcher(Groups *groups)
{
    groups_ptr = groups;
    ceiling_event = floor_event = nullptr;
}

std::map<EventType::event_type_t, bool> BugSearcher::add_key_events()
{
    std::map<EventType::event_type_t, bool> key_events;
    key_events.insert(std::make_pair(MACH_IPC_MSG, true));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_INFO, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_CONN, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_TRANSIT, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_DEALLOC, false));
    key_events.insert(std::make_pair(MACH_BANK_ACCOUNT, false));
    key_events.insert(std::make_pair(MACH_MK_RUN, false));
    key_events.insert(std::make_pair(INTR, false));
    key_events.insert(std::make_pair(WQ_NEXT, false));
    key_events.insert(std::make_pair(MACH_TS, false));
    key_events.insert(std::make_pair(MACH_WAIT, false));
    key_events.insert(std::make_pair(DISP_ENQ, true));
    key_events.insert(std::make_pair(DISP_DEQ, true));
    key_events.insert(std::make_pair(DISP_EXE, true));
    key_events.insert(std::make_pair(MACH_CALLCREATE, true));
    key_events.insert(std::make_pair(MACH_CALLOUT, true));
    key_events.insert(std::make_pair(MACH_CALLCANCEL, true));
    key_events.insert(std::make_pair(BACKTRACE, false));
    key_events.insert(std::make_pair(MACH_SYS, true));
    key_events.insert(std::make_pair(BSD_SYS, true));
    return key_events;
}

EventBase *BugSearcher::search_source_event_in_main_thread(int event_type, EventBase *event)
{
    EventBase *cur_event, *target_event = nullptr;
    std::list<EventBase *> event_list = groups_ptr->get_list_of_tid(groups_ptr->get_main_thread());
    std::list<EventBase *>::reverse_iterator rit;
    for (rit = event_list.rbegin(); rit != event_list.rend(); rit++) {
        cur_event = *rit;
        if (cur_event->get_abstime() < event->get_abstime()
                && cur_event->get_event_type() == event_type) {
            switch (event_type) {
                case BREAKPOINT_TRAP_EVENT: {
                    BreakpointTrapEvent *hwbr_event = dynamic_cast<BreakpointTrapEvent *>(cur_event);
                    if (hwbr_event->get_trigger_var() == "_ZL32sCGEventIsDispatchedToMainThread" && hwbr_event->get_trigger_val() == 0)
                        target_event = cur_event;
                    break;
                }
                default:
                    break;
            }
        }
        if (target_event)
            break;
    }
    return target_event;
}

cluster_t *BugSearcher::search_cluster_overlap(EventBase *event)
{
    Group *group = groups_ptr->group_of(event);

#if DEBUG_SEARCH_ENGINE
    mtx.lock();
    if (group == nullptr)
        std::cerr << "Event at " << std::fixed << std::setprecision(1) << event->get_abstime() << "is not grouped" << std::endl;
    else if (group->get_cluster_idx() == -1)
        std::cerr << "Event at " << std::fixed << std::setprecision(1) << event->get_abstime() << "is not clustered" << std::endl;
    mtx.unlock();
#endif

    if (group && group->get_cluster_idx() != -1)
        return cluster_map[group->get_cluster_idx()];

    return nullptr;
}

cluster_t *BugSearcher::report_spinning_cluster()
{
    std::list<EventBase *> hwbr_list = groups_ptr->get_list_of_op(BREAKPOINT_TRAP);
    std::list<EventBase *>::iterator it;
    for (it = hwbr_list.begin(); it != hwbr_list.end(); it++) {
        BreakpointTrapEvent *hwbr_event = dynamic_cast<BreakpointTrapEvent*>(*it);
        if (hwbr_event->get_trigger_var() == "_ZL28sCGEventIsMainThreadSpinning"
            && hwbr_event->get_trigger_val() == 1) {
            floor_event = hwbr_event;
            ceiling_event = search_source_event_in_main_thread(BREAKPOINT_TRAP_EVENT, hwbr_event);
            break;
        }
    }
    if (floor_event && ceiling_event) {
#if DEBUG_SEARCH_ENGINE
    mtx.lock();
    std::cerr << "Beachball happens between " << std::fixed << std::setprecision(1) << ceiling_event->get_abstime() \
        << " and " << std::fixed <<std::setprecision(1) << floor_event->get_abstime() << std::endl;
    mtx.unlock();
#endif
        return search_cluster_overlap(ceiling_event);
    }
#if DEBUG_SEARCH_ENGINE
    mtx.lock();
    std::cerr << "No beachball cluster reported" << std::endl; 
    mtx.unlock();
#endif
    return nullptr;
}

template <typename T>
T BugSearcher::max_item_key(std::map<T, double> &maps, T comb)
{
    double max = 0.0;
    T max_key = comb;
    typenamestd::map<T, double>::iterator it;
    for (it =std::maps.begin(); it !=std::maps.end(); it++) {
        if (it->second > max) {
            max = it->second;
            max_key = it->first;
        }
    }
    return max_key;
}

WaitEvent *BugSearcher::suspicious_blocking(cluster_t *cluster)
{
    std::list<WaitEvent *> &wait_events = cluster->get_wait_events();
     std::list<WaitEvent *>::iterator it;
    std::list<WaitEvent *> unknown_blockings;
    std::map<WaitEvent *, double> blocking_time_span;
    WaitEvent *wait_event;

    for (it = wait_events.begin(); it != wait_events.end(); it++) {
        wait_event = *it;
        if (wait_event->get_tid() != groups_ptr->get_main_thread())
            continue;
        SyscallEvent *syscall_event = wait_event->get_syscall();
        if (syscall_event) {
            if (syscall_event->get_ret_time() > 0)
                blocking_time_span[wait_event] = syscall_event->get_ret_time() - syscall_event->get_abstime();
            else
                unknown_blockings.push_back(wait_event);
        } else {
            unknown_blockings.push_back(wait_event);
        }
    }
#if DEBUG_SEARCH_ENGINE
    mtx.lock();
    for (it = unknown_blockings.begin(); it != unknown_blockings.end(); it++) {
        std::cerr << "unknown blocking time for wait at " << std::fixed << std::setprecision(1) << (*it)->get_abstime() << std::endl;
    }
    mtx.unlock();
#endif
    return max_item_key(blocking_time_span, (WaitEvent *)nullptr);
}

Group *BugSearcher::suspicious_segment(cluster_t *cluster)
{
    std::vector<Group *> &container = cluster->get_nodes();
    std::vector<Group *>::iterator it;
    std::map<Group *, double> Groupime_span;

    for (it = container.begin(); it != container.end(); it++)
        Groupime_span[(*it)] = (*it)->calculate_time_span();

    return max_item_key(Groupime_span, (Group *)nullptr);
}

//////////////////////////////////////////////////////
std::map<WaitEvent*, double> BugSearcher::suspicious_blocking(Groups *groups, std::string outfile)
{
    std::list<EventBase *> &wait_events = groups->get_wait_events();
    std::list<EventBase *>::iterator it;
    std::map<WaitEvent *, double> blocking_time_span;
    WaitEvent *wait_event;

    std::ofstream output(outfile);

    for (it = wait_events.begin(); it != wait_events.end(); it++) {
        assert(*it);
        wait_event = dynamic_cast<WaitEvent *>(*it);
        assert(wait_event);
        EventBase *wakeup = wait_event->get_mkrun();
        if (wakeup != nullptr) { 
            if(wakeup->get_abstime() - wait_event->get_abstime() > 1000000) {
                blocking_time_span[wait_event] = wakeup->get_abstime() - wait_event->get_abstime();
                wait_event->streamout_event(output);
            }
        } else {
            blocking_time_span[wait_event] = -1;
            wait_event->streamout_event(output);
        }
    }
    output.close();
    return blocking_time_span;
}

void BugSearcher::update_normalized_map_for_thread(thread_id_t thread)
{
    if (normalized_groups_map[thread].size() != 0)
        return;
    
    std::map<uint64_t, Group *> &groups_list =  groups_ptr->get_groups_by_tid(thread);
    std::map<EventType::event_type_t, bool> key_events = add_key_events();
    std::map<uint64_t, Group *>::iterator it;
    for (it = groups_list.begin(); it != groups_list.end(); it++) {
        normalized_groups_map[thread][it->first] = new NormGroup(it->second, key_events);
    }
}

std::vector<uint64_t> BugSearcher::get_counterpart(uint64_t group_id, uint64_t tid)
{
    std::vector<uint64_t> group_ids;
    update_normalized_map_for_thread(tid);
    NormGroup target = *(normalized_groups_map[tid][group_id]);
    std::map<uint64_t, NormGroup *>::iterator it;
    for (it = normalized_groups_map[tid].begin(); it != normalized_groups_map[tid].end(); it++) {
        if (it->first >= group_id)
            break;
        if (*(it->second) == target)
            group_ids.push_back(it->first);
    }
    return group_ids;
}

void BugSearcher::slice_path(uint64_t group_id, std::string outfile)
{
    std::ofstream output(outfile);
    Group *cur_group = groups_ptr->spinning_group();
    EventBase *cur_event = nullptr;

    if (cur_group) 
        group_id = cur_group->get_group_id();
    return;

    for(;;) {
        if (group_id == -1) {
            std::cout << "Input the groupid [exit with 0]:" << std::endl;
            std::cin >> std::hex >> group_id;
        }
        if (group_id <= 0)
            break;
        
        std::cout << "Path search from #Group " << std::hex << group_id << std::endl;
        cur_group = groups_ptr->get_group_by_gid(group_id);
        cur_event = cur_group->get_first_event();
        cur_group->decode_group(output);

        std::vector<uint64_t> counterparts = get_counterpart(group_id, cur_event->get_tid());
        std::vector<uint64_t>::iterator it;
        if (counterparts.size() > 0) {
            std::cout << "Similar groups before:" << std::endl;
            for (it = counterparts.begin(); it != counterparts.end(); it++) {
                std::cout << "#Group " << std::hex << *it << std::endl;
                groups_ptr->get_group_by_gid(*it)->pic_group(std::std::cout);
            }
        } else {
            std::cout << "No similar groups happened before. Lets check itself" << std::endl;
        }
        // print the group in the user space
        std::cout << "Current group: " << std::endl;
        cur_group->pic_group(std::std::cout);

        if (cur_event->get_event_type() == FAKED_WOKEN_EVENT) {
            FakedWokenEvent *fakedevent = dynamic_cast<FakedWokenEvent *>(cur_event);
            group_id = fakedevent->get_peer()->get_group_id();
            std::cout << "Find the proceeding group #Group " << std::hex << group_id << std::endl;
            
        } else {
            //search all the possible edge from other group
            std::cout << "No processing group found for #Group " << std::hex << group_id << std::endl;
            group_id = -1;
        }
    }
    output.close();
}
