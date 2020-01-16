#include "group.hpp"
#include "thread_divider.hpp"
#include <time.h>

#define DEBUG_GROUP    0

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

pid_t ListsCategory::tid2pid(uint64_t tid)
{
    if (LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end())
        return LoadData::tpc_maps[tid]->get_pid();
    return (pid_t)-1;
}

void ListsCategory::prepare_list_for_tid(tid_t tid, tid_evlist_t &tid_list_map)
{
    if (tid_list_map.find(tid) != tid_list_map.end())
        return;
    event_list_t l;
    l.clear();
    tid_list_map[tid] = l;
}

void ListsCategory::threads_wait_graph(event_list_t &ev_list)
{
    tid_evlist_t tid_list_map;
    std::map<uint64_t, MakeRunEvent *> who_make_me_run_map;
    MakeRunEvent *mr_event;
    uint64_t tid, peer_tid;
    event_list_t::iterator it;

    for (it = ev_list.begin(); it != ev_list.end(); it++) {
        tid = (*it)->get_tid();
        prepare_list_for_tid(tid, tid_list_map);

        /* record causality across thread boundary via make runnable */
        if (who_make_me_run_map.find(tid) != who_make_me_run_map.end()) {
            FakedWokenEvent *i_am_running_now = new FakedWokenEvent((*it)->get_abstime() - 0.01,
                "faked_woken", (*it)->get_tid(), who_make_me_run_map[tid],
                (*it)->get_coreid(), (*it)->get_procname());
            who_make_me_run_map[tid]->set_event_peer(i_am_running_now);
            tid_list_map[tid].push_back(i_am_running_now);
            ev_list.insert(it, i_am_running_now);
            who_make_me_run_map.erase(tid);
        }

        tid_list_map[tid].push_back(*it);

        if ((mr_event = dynamic_cast<MakeRunEvent *>(*it))) {
            peer_tid = mr_event->get_peer_tid();
#ifdef DEBUG_GROUP
            mtx.lock();
            if (who_make_me_run_map.find(peer_tid) != who_make_me_run_map.end()) {
                std::cerr << "Warn: multiple make runnable " << std::fixed << std::setprecision(1) \
                    << mr_event->get_abstime() << std::endl;
                std::cerr << "\tPrevious make runnalbe at " << std::fixed << std::setprecision(1) \
                    << who_make_me_run_map[peer_tid]->get_abstime() << std::endl;
            }
            mtx.unlock();
#endif
            who_make_me_run_map[peer_tid] = mr_event;
        }
    }
    tid_lists = tid_list_map;
}

bool ListsCategory::interrupt_mkrun_pair(EventBase *cur, event_list_t::reverse_iterator rit)
{
    EventBase *pre = *(++rit);

    if (cur->get_event_type() != MR_EVENT)
        return false;
    
    switch (pre->get_event_type()) {
        case INTR_EVENT: {
            MakeRunEvent *mkrun = dynamic_cast<MakeRunEvent *>(cur);
            IntrEvent *interrupt = dynamic_cast<IntrEvent *>(pre);
            return mkrun->check_interrupt(interrupt);
        }
        case TSM_EVENT:
            return true;
        default:
            return false;
    }
}

int ListsCategory::remove_sigprocessing_events(event_list_t &event_list, event_list_t::reverse_iterator rit)
{
    event_list_t::iterator remove_pos;
    int sigprocessing_seq_reverse[] = {MSG_EVENT,
                    MSG_EVENT,
                    SYSCALL_EVENT,
                    BACKTRACE_EVENT, /*get_thread_state*/
                    WAIT_EVENT,
                    MR_EVENT,
                    MSG_EVENT}; /*wake up by cpu signal from kernel thread*/

    int i = 0, remove_count = 0, n = sizeof(sigprocessing_seq_reverse) / sizeof(int);

    while (rit != event_list.rend() && (*rit)->get_event_type() != MSG_EVENT)
        rit++;

    for (; rit != event_list.rend() && i < n; i++) {
    retry:
        while (rit != event_list.rend()) {
            if ((*rit)->get_event_type() == FAKED_WOKEN_EVENT) {
                remove_pos = next(rit).base();
                assert(*remove_pos == *rit);
                event_list.erase(remove_pos);
                remove_count++;
            } else if ((*rit)->get_event_type() == INTR_EVENT) {
                rit++;
            } else if (interrupt_mkrun_pair((*rit), rit)) {
                rit++;
                if (rit == event_list.rend()) 
                    return remove_count;
                rit++;
            } else {
                break;
            }
        }
        
        if (rit == event_list.rend()) 
            return remove_count;

        if (sigprocessing_seq_reverse[i] == WAIT_EVENT) {
            if (((*rit)->get_event_type() == MR_EVENT && !interrupt_mkrun_pair((*rit), rit)) 
                || (*rit)->get_event_type() == MSG_EVENT) {
#if DEBUG_GROUP
                mtx.lock();
                std::cerr << "skip wait event at " \
                    << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                mtx.unlock();
#endif
                continue;
            } else if ((*rit)->get_op() == "MSC_mach_reply_port") {
#if DEBUG_GROUP
                mtx.lock();
                std::cerr << "remove event at "\
                    << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                mtx.unlock();
#endif
                remove_pos = next(rit).base();
                assert(*remove_pos == *rit);
                event_list.erase(remove_pos);
                remove_count++;
                if (rit == event_list.rend()) 
                    return remove_count;
                goto retry;    
            } 
        }

        if ((*rit)->get_event_type() == sigprocessing_seq_reverse[i]) {
#if DEBUG_GROUP
            mtx.lock();
            std::cerr << "remove event " << i << " at "\
                << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
            mtx.unlock();
#endif
            remove_pos = next(rit).base();
            assert(*remove_pos == *rit);
            event_list.erase(remove_pos);
            remove_count++;
        } else {
            // skip wake the kernel thread, remove the sent message to the kernel thread
            if (sigprocessing_seq_reverse[i] == MR_EVENT && (*rit)->get_event_type() == MSG_EVENT) {
                MsgEvent *msg_event = dynamic_cast<MsgEvent *>(*rit);
                if (msg_event->get_header()->check_recv() == false) { 
                    remove_pos = next(rit).base();
                    assert(*remove_pos == *rit);
                    event_list.erase(remove_pos);
                    remove_count++;
                } 
#ifdef DEBUG_GROUP
                else {
                    mtx.lock();
                    std::cerr << "no matching mr event " << i << " at "\
                         << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                    mtx.unlock();
                }
            } else {
                mtx.lock();
                std::cerr << "no matching event " << i << " at "\
                        << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                mtx.unlock();
#endif
            }
            return remove_count;
        }
    }
    return remove_count;
}

ProcessInfo *ListsCategory::get_process_info(tid_t tid, EventBase *front)
{
    if (LoadData::tpc_maps.find(tid) == LoadData::tpc_maps.end()) {
        ProcessInfo *p = new ProcessInfo(tid, tid2pid(tid), front->get_procname());
        LoadData::tpc_maps[tid] = p;
    }
    return LoadData::tpc_maps[tid];
}

void ListsCategory::update_events_in_thread(uint64_t tid)
{
    event_list_t &tid_list = tid_lists[tid]; 
    event_list_t::iterator it;
    EventBase *event, *prev = nullptr;
    ProcessInfo *p = get_process_info(tid, tid_list.front());
    NSAppEventEvent *appevent, *appevent_begin = nullptr;

    for (it = tid_list.begin(); it != tid_list.end(); it++) {
        event = *it;
        event->update_process_info(*p);
        event->set_event_prev(prev);
        prev = event;

        /* update nsappevent of begin which does not record detail*/
        if (event->get_event_type() == NSAPPEVENT_EVENT) {
            appevent = dynamic_cast<NSAppEventEvent*>(event);
            if (appevent->is_begin()) {
                appevent_begin = appevent;
            } else if (appevent_begin != nullptr) {
                appevent_begin->set_event(appevent->get_event_class(), appevent->get_key_code());
                appevent_begin = nullptr;
            } 
        }
        
        /* remove events related to hwbr trap signal processing */
        if (event->get_event_type() == BREAKPOINT_TRAP_EVENT) {
            int dist = distance(tid_list.begin(), it);
            event_list_t::reverse_iterator rit(next(it));
            assert(*rit == *it);
            int removed_count = remove_sigprocessing_events(tid_list, rit);
            dist -= removed_count;
            assert(dist <= tid_list.size());
            it = tid_list.begin();
            advance(it, dist);
            assert((*it) == event);
        }

        /*remove Interrupt/timeshare_maintainance event waken*/
        if (event->get_event_type() == TSM_EVENT
            && next(it) != tid_list.end()
            && (*(next(it)))->get_event_type() == MR_EVENT) {
            it = tid_list.erase(it); // TSM
            assert((*it)->get_event_type() == MR_EVENT);
            it = tid_list.erase(it); // MR
            if (it != tid_list.begin())
                it = std::prev(it);
            event = *it;
        }

retry:
        if (event->get_event_type() == INTR_EVENT 
            && next(it) != tid_list.end()
            && (*(next(it)))->get_event_type() == MR_EVENT) {
            MakeRunEvent *mkrun = dynamic_cast<MakeRunEvent *>(*(next(it)));
            IntrEvent *interrupt = dynamic_cast<IntrEvent *>(event);
            if (mkrun->check_interrupt(interrupt) == false)
               continue;
            it = tid_list.erase(it); // INTR
            assert((*it)->get_event_type() == MR_EVENT);
            it = tid_list.erase(it); // MR
            if (it != tid_list.begin())
                it = std::prev(it);
            else {
                event = *it;
                goto retry;
            }
        } 
    }
#if DEBUG_GROUP
    mtx.lock();
    std::cerr << "in update_event thread " << std::hex << tid << " size " << tid_list.size() << std::endl;
    mtx.unlock();
#endif
}
//////////////////////////////////////////

void ThreadType::check_rlthreads(event_list_t &rlboundary_list, event_list_t &rlobserver_list)
{
    uint64_t tid;
    RunLoopBoundaryEvent *rl_boundary_event;
    std::set<uint64_t> rl_entries;
    event_list_t::iterator it;

    for (it = rlboundary_list.begin(); it != rlboundary_list.end(); it++) {
        rl_boundary_event = dynamic_cast<RunLoopBoundaryEvent *>(*it);
        tid = rl_boundary_event->get_tid();
        if (rl_entries.find(tid) != rl_entries.end())
            continue;
        if (rl_boundary_event->get_state() == ItemBegin)
            rl_entries.insert(tid);
    }
    categories[RunLoopThreadType] = rl_entries;

    RunLoopObserverEvent *rl_observer_event;
    std::set<uint64_t> with_observer_entry;
    std::set<uint64_t> wo_observer_entry;

    for (it = rlobserver_list.begin(); it != rlobserver_list.end(); it++) {
        rl_observer_event = dynamic_cast<RunLoopObserverEvent *>(*it);
        tid = rl_observer_event->get_tid();
        
        switch (rl_observer_event->get_stage()) {
            case kCFRunLoopEntry:
                with_observer_entry.insert(tid);
                break;
            case kCFRunLoopExtraEntry:
                if (with_observer_entry.find(tid) == with_observer_entry.end())
                    wo_observer_entry.insert(tid);
                else
                    wo_observer_entry.erase(tid);
                break;
            default:
                break;
        }
    }
    categories[RunLoopThreadType_WITH_OBSERVER_ENTRY] = with_observer_entry;
    categories[RunLoopThreadType_WITHOUT_OBSERVER_ENTRY] = wo_observer_entry;

}

void ThreadType::check_host_uithreads(event_list_t &backtrace_list)
{
    std::string mainthread("[NSApplication run]");
    std::string mainthreadobserver("MainLoopObserver");
    std::string mainthreadsendevent("SendEventToEventTargetWithOptions");
    std::string nsthread("_NSEventThread");
    BacktraceEvent *backtrace_event;
    event_list_t::iterator it;
    uint64_t tid;

    for (it = backtrace_list.begin(); it != backtrace_list.end()
            && (main_thread == -1 || nsevent_thread == -1); it++) {
        backtrace_event = dynamic_cast<BacktraceEvent *>(*it);
        tid = (*it)->get_tid();

        if ((*it)->get_procname() != LoadData::meta_data.host
            || tid == main_thread
            || tid == nsevent_thread)
            continue;

        if (main_thread == -1
            && (backtrace_event->contains_symbol(mainthread)
                || backtrace_event->contains_symbol(mainthreadobserver)
                || backtrace_event->contains_symbol(mainthreadsendevent))) {
                main_thread = tid;
        } else if (nsevent_thread == -1
            && backtrace_event->contains_symbol(nsthread)) {
            nsevent_thread = tid;
        }
    }
#ifdef DEBUG_GROUP
    mtx.lock();
    std::cerr << "Mainthread " << std::hex << main_thread << std::endl;
    std::cerr << "Eventthread " << std::hex << nsevent_thread << std::endl;
    mtx.unlock();
#endif
}


/////////////////////////////////////////
Groups::Groups(EventLists *eventlists_ptr)
:ListsCategory(eventlists_ptr->get_event_lists()),
ThreadType()
{
    groups.clear();
    main_groups.clear();
    host_groups.clear();
    sub_results.clear();

    for (tid_evlist_t::iterator it = tid_lists.begin(); it != tid_lists.end(); it++) {
        std::map<uint64_t, Group *> temp;
        temp.clear();
        sub_results[it->first] = temp;
    }
    init_groups();

    //categories.clear();
    //main_thread = nsevent_thread = -1;
}

Groups::Groups(op_events_t &_op_lists)
:ListsCategory(_op_lists),
ThreadType()
{
    groups.clear();
    main_groups.clear();
    host_groups.clear();
    sub_results.clear();

    for (tid_evlist_t::iterator it = tid_lists.begin(); it != tid_lists.end(); it++) {
        std::map<uint64_t, Group *> temp;
        temp.clear();
        sub_results[it->first] = temp;
    }
    init_groups();
}

Groups::Groups(Groups &copy_groups)
:ListsCategory(copy_groups.get_op_lists()),
ThreadType()
{
    sub_results.clear();
    groups = copy_groups.get_groups();
    main_groups = copy_groups.get_main_groups();
    host_groups = copy_groups.get_host_groups();

    std::map<uint64_t, Group *>::iterator it;
    Group *this_group, *copy_group;

    for (it = groups.begin(); it != groups.end(); it++) {
        copy_group = it->second;
        this_group = new Group(*copy_group);
        groups[it->first] = this_group;
        if (main_groups.find(it->first) != main_groups.end())
            main_groups[it->first] = this_group;
        if (host_groups.find(it->first) != host_groups.end())
            host_groups[it->first] = this_group;
    } 
}

Groups::~Groups(void)
{

    std::map<uint64_t, Group *>::iterator it;
    Group *cur_group;

    for (it = groups.begin(); it != groups.end(); it++) {
        cur_group = it->second;
        assert(cur_group);
        delete(cur_group);
    }
    groups.clear();
    main_groups.clear();
    host_groups.clear();
    sub_results.clear();
}

void Groups::init_groups()
{

    /*1. divide event list */
    para_preprocessing_tidlists();
    para_identify_thread_types();

    /*2. para-processing connector peers */
    para_connector_generate();

    /*3. para-grouping */
    para_group();
}


void Groups::para_preprocessing_tidlists(void)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));
    
    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    tid_evlist_t::iterator it;
    for (it = tid_lists.begin(); it != tid_lists.end(); it++)
        ioService.post(boost::bind(&ListsCategory::update_events_in_thread, this, it->first));
    work.reset();
    threadpool.join_all();
}

void Groups::para_identify_thread_types(void)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));
    
    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
    ioService.post(boost::bind(&ThreadType::check_host_uithreads, this, get_list_of_op(BACKTRACE)));
    ioService.post(boost::bind(&ThreadType::check_rlthreads, this, get_list_of_op(RL_BOUNDARY), get_list_of_op(RL_OBSERVER)));
    
    work.reset();
    threadpool.join_all();
    //ioService.stop();
}

void Groups::para_connector_generate(void)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));
    
    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    /*pair mach msg*/
    MsgPattern msg_pattern(get_list_of_op(MACH_IPC_MSG));
    ioService.post(boost::bind(&MsgPattern::collect_patterned_ipcs, msg_pattern));

    /*pair dispatch queue*/
    DispatchPattern dispatch_pattern(get_list_of_op(DISP_ENQ),
            get_list_of_op(DISP_DEQ),
            get_list_of_op(DISP_EXE));
    ioService.post(boost::bind(&DispatchPattern::connect_dispatch_patterns,
                dispatch_pattern));

    /*pair timer callout*/
    TimerCallPattern timercall_pattern(get_list_of_op(MACH_CALLCREATE),
            get_list_of_op(MACH_CALLOUT),
            get_list_of_op(MACH_CALLCANCEL));
    ioService.post(boost::bind(&TimerCallPattern::connect_timercall_patterns,
                timercall_pattern));

    /*match core animation*/
    CoreAnimationConnection core_animation_connection (get_list_of_op(CA_SET),
            get_list_of_op(CA_DISPLAY));
    ioService.post(boost::bind(&CoreAnimationConnection::core_animation_connection , core_animation_connection ));

    /*match breakpoint_trap connection*/
    BreakpointTrapConnection breakpoint_trap_connection(get_list_of_op(BREAKPOINT_TRAP));
    ioService.post(boost::bind(&BreakpointTrapConnection::breakpoint_trap_connection, breakpoint_trap_connection));

    /*match rl work*/
    RunLoopTaskConnection runloop_connection(get_list_of_op(RL_BOUNDARY), tid_lists);
    ioService.post(boost::bind(&RunLoopTaskConnection::runloop_connection, runloop_connection));

    /*fill wait -- makerun info*/
    MakeRunnableWaitPair mkrun_wait_pair(get_list_of_op(MACH_WAIT),
            get_list_of_op(MACH_MK_RUN));
    ioService.post(boost::bind(&MakeRunnableWaitPair::pair_wait_mkrun,
                mkrun_wait_pair));

    /*fill voucher info*/
    VoucherBankAttrs voucher_bank_attrs(get_list_of_op(MACH_BANK_ACCOUNT),
            get_list_of_op(MACH_IPC_VOUCHER_INFO));
    ioService.post(boost::bind(&VoucherBankAttrs::update_event_info,
                voucher_bank_attrs));

    work.reset();
    threadpool.join_all();
    //ioService.stop();
}

void Groups::para_group(void)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));
    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
    
    uint64_t idx = 0, main_idx = -1;
    std::vector<uint64_t> host_idxes;
    host_idxes.clear();

    tid_evlist_t::iterator it;

    for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
        if ((it->second).size() == 0)
            continue;
        idx = it->first;
        if (LoadData::pid2comm(tid2pid(it->first)) == LoadData::meta_data.host)
            host_idxes.push_back(idx);

        if (categories[RunLoopThreadType_WITH_OBSERVER_ENTRY].find(it->first)
             != categories[RunLoopThreadType_WITH_OBSERVER_ENTRY].end())  {
            RunLoopThreadDivider rl_thread_divider(idx, sub_results, it->second, false);
            //rl_thread_divider.divide();
            ioService.post(boost::bind(&RunLoopThreadDivider::divide, rl_thread_divider));
            if (it->first == main_thread)
                main_idx = idx;
        } else if (categories[RunLoopThreadType_WITHOUT_OBSERVER_ENTRY].find(it->first)
            != categories[RunLoopThreadType_WITHOUT_OBSERVER_ENTRY].end())  {
            RunLoopThreadDivider rl_thread_divider(idx, sub_results, it->second, true);
            //rl_thread_divider.divide();
            ioService.post(boost::bind(&RunLoopThreadDivider::divide, rl_thread_divider));
            
        } else if (categories[RunLoopThreadType].find(it->first) != categories[RunLoopThreadType].end()) {
            RunLoopThreadDivider rl_thread_divider(idx, sub_results, it->second, false);
            //rl_thread_divider.divide();
            ioService.post(boost::bind(&RunLoopThreadDivider::divide, rl_thread_divider));
        } else {
            ThreadDivider general_thread_divider(idx, sub_results, it->second);
            //general_thread_divider.divide();
            ioService.post(boost::bind(&ThreadDivider::divide, general_thread_divider));
        }
    }
    
    work.reset();
    threadpool.join_all();
    //ioService.stop();
    
    std::map<uint64_t,std::map<uint64_t, Group *>>::iterator ret_it;
    //std::vector<std::map<uint64_t, Group *>>::iterator ret_it;
    for (ret_it = sub_results.begin(); ret_it != sub_results.end(); ret_it++)
        collect_groups(ret_it->second);
    
    std::vector<uint64_t>::iterator idx_it;
    for (idx_it = host_idxes.begin(); idx_it != host_idxes.end(); idx_it++)
        host_groups.insert(sub_results[*idx_it].begin(), sub_results[*idx_it].end());

    if (main_idx != -1)
        main_groups = sub_results[main_idx];
}

void Groups::collect_groups(std::map<uint64_t, Group *> &sub_groups)
{
    if (sub_groups.size())
        groups.insert(sub_groups.begin(), sub_groups.end());
}

Group *Groups::group_of(EventBase *event)
{
    assert(event);
    uint64_t gid = event->get_group_id();
    if (groups.find(gid) != groups.end()) { 
        return groups[gid];
    } 
    return nullptr;
}


Group *Groups::spinning_group()
{
    std::cout << "Try to detect spinning group" << std::endl;
    if (nsevent_thread == -1) {
        return nullptr;
    }

    double nsthread_spin_timestamp  = 0.0;
    event_list_t backtrace_events = get_list_of_op(BACKTRACE);
    event_list_t::reverse_iterator rit;

    for (rit = backtrace_events.rbegin(); rit != backtrace_events.rend(); rit++) {
        BacktraceEvent *backtrace = dynamic_cast<BacktraceEvent *>(*rit);
        if (backtrace->spinning() == true) {
            std::cout << "Spindetect: " << std::hex << backtrace->get_group_id() << std::endl;
            nsthread_spin_timestamp = backtrace->get_abstime();
            break;
        }
    }
    
    if (nsthread_spin_timestamp > 0.0) {
        std::cout << "spin at " << std::fixed << nsthread_spin_timestamp << std::endl;
        std::map<uint64_t, Group *>mainthread_groups = sub_results[main_thread];
        std::map<uint64_t, Group *>::reverse_iterator rit;
        mtx.lock();
        std::cerr << "Main thread has " << mainthread_groups.size() << " groups" << std::endl;
        mtx.unlock();
        
        for (rit = mainthread_groups.rbegin(); rit != mainthread_groups.rend(); rit++) {
            Group *cur_group = rit->second;
            if (cur_group->get_first_event()->get_abstime() < nsthread_spin_timestamp) {
                mtx.lock();
                std::cerr << "Spinning at " << nsthread_spin_timestamp << std::endl;
                std::cerr << "Corresponding busy UI execution: Group #" << std::hex << cur_group->get_group_id() << std::endl;
                mtx.unlock();
                return cur_group;
            }
        }
    }
    std::cout << "Spinning group is not found" << std::endl;
    return nullptr;
}

bool Groups::matched(Group *target)
{
#if DEBUG_GROUP
    mtx.lock();
    std::cerr << "match group " << std::hex <<  target->get_group_id() << std::endl;
    mtx.unlock();
#endif
    std::map<uint64_t, Group *>::iterator it;

    for (it = groups.begin(); it != groups.end(); it++) {
        if (*(it->second) == *target) {
#if DEBUG_GROUP
            mtx.lock();
            std::cerr << "group " << std::hex << target->get_group_id() << " matched" << std::endl;
            mtx.unlock();
#endif
            return true;
        }
    }
    return false;
}

void Groups::partial_compare(Groups *peer_groups, std::string proc_name, std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, Group *>::iterator it;
    for (it = groups.begin(); it != groups.end(); it++) {
        if (proc_name.size() != 0 && it->second->get_first_event()->get_procname() != proc_name)
            continue;
            if (!peer_groups->matched(it->second)) {
                //record to the file
                it->second->streamout_group(output);        
            }
    }
    output.close();
}

int Groups::decode_groups(std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, Group *>::iterator it;

    output << "Total number of Groups = " << groups.size() << std::endl << std::endl;
    for (it = groups.begin(); it != groups.end(); it++) {
        Group *cur_group = it->second;
        output << "#Group " << std::hex << cur_group->get_group_id();
        output << "(length = " << std::hex << cur_group->get_size() << "):\n";
        cur_group->decode_group(output);
    }
    output.close();
    return 0;
}

int Groups::streamout_groups(std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, Group *>::iterator it;

    output << "Total number of Groups = " << groups.size() << std::endl;
    for (it = groups.begin(); it != groups.end(); it++) {
        Group *cur_group = it->second;
        //if (cur_group->get_size() <= 2)
          //  continue;
        output << "#Group " << std::hex << cur_group->get_group_id();
        output << "(length = " << std::hex << cur_group->get_size() <<"):\n";
        cur_group->streamout_group(output);
    }
    output.close();
    return 0;
}

/////////////////////////////////////////////
/*checking can also be done paralelly*/
bool Groups::check_interleaved_pattern(event_list_t &ev_list, event_list_t::iterator &it)
{
    MsgEvent *msg_event = dynamic_cast<MsgEvent *>(*it);
    assert(msg_event);
    bool found = false;
    mtx.lock();
    std::cerr << "Check interleaved msg receive at: "<< std::fixed << std::setprecision(1) << msg_event->get_abstime() << std::endl;
    mtx.unlock();
    for (it++; it != ev_list.end()
        && (*it)->get_abstime() < msg_event->get_next()->get_abstime(); it++) {
        MsgEvent *other = nullptr;
        if ((other = dynamic_cast<MsgEvent *>(*it)) 
            && other->get_header()->is_mig() == false
            && other->get_peer()
            && other->get_peer()->get_tid() != msg_event->get_next()->get_tid()
            && other->get_peer()->get_tid() != msg_event->get_tid()) {
            mtx.lock();
            std::cerr << "Interleaved procs: " << msg_event->get_procname() \
                << "\t" << other->get_peer()->get_procname() << " at " \
                << std::fixed << std::setprecision(1) << other->get_abstime() \
                << "\t" << msg_event->get_peer()->get_procname() << std::endl;
            mtx.unlock();
            found = true;
        }
    }
    if (found == true) {
        mtx.lock();
        std::cerr << "Interleaved patterns for " << msg_event->get_procname();
        std::cerr << "\treceive: "<< std::fixed << std::setprecision(1) << msg_event->get_abstime();
        std::cerr << "\treply: " <<std::fixed << std::setprecision(1) << msg_event->get_next()->get_abstime() << std::endl;
        mtx.unlock();
    }
    return found;
}

void Groups::check_interleavemsg_from_thread(event_list_t &evlist)
{
    event_list_t::iterator ev_it;
    for (ev_it = evlist.begin(); ev_it != evlist.end(); ev_it++) {
        MsgEvent *msg_event = dynamic_cast<MsgEvent *>(*ev_it);
        if (!msg_event || !msg_event->get_header()->check_recv())
            continue;

        if (!msg_event->get_next())
            continue;
        if (msg_event->get_next()->get_tid() == msg_event->get_tid())
            check_interleaved_pattern(evlist, ev_it);
    }
}

void Groups::check_msg_pattern()
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));
    
    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    //std::ofstream output(output_path);
    tid_evlist_t::iterator it;
    event_list_t::iterator ev_it;
    std::cerr << "Checking mach message interleave inside execution segment" << std::endl;
    for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
        if ((it->second).size() == 0)
            continue;
        ioService.post(boost::bind(&Groups::check_interleavemsg_from_thread, this, it->second));
    }
    work.reset();
    threadpool.join_all();
}
