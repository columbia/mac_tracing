#include "group.hpp"
#include "thread_divider.hpp"
#include <time.h>

#define DEBUG_GROUP    0

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

pid_t ListsCategory::tid2pid(uint64_t tid)
{	
	pid_t ret = -1;
	assert(LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end());
	if (LoadData::tpc_maps[tid] != nullptr)
        ret = LoadData::tpc_maps[tid]->get_pid();
	return ret;
}

void ListsCategory::prepare_list_for_tid(tid_t tid, tid_evlist_t &tid_list_map)
{
	if (LoadData::tpc_maps.find(tid) == LoadData::tpc_maps.end())
		LoadData::tpc_maps[tid] = nullptr;

    if (tid_list_map.find(tid) != tid_list_map.end())
        return;

    event_list_t l;
    l.clear();
    tid_list_map[tid] = l;
}

// A sequential iteration of all events to prepare maps 
// for lock free multi-thread processing 
void ListsCategory::threads_wait_graph(event_list_t &ev_list)
{
    tid_evlist_t tid_list_map;
    std::map<uint64_t, MakeRunEvent *> who_make_me_run_map;
    MakeRunEvent *mr_event;
    uint64_t tid, peer_tid;

    event_list_t::iterator it;
    for (it = ev_list.begin(); it != ev_list.end(); it++) {
		assert((*it)->get_event_type() > 0);
        tid = (*it)->get_tid();
        prepare_list_for_tid(tid, tid_list_map);
		assert(tid_list_map.find(tid) != tid_list_map.end());

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
		assert((*it)->get_event_type() > 0);

		if ((*it)->get_event_type() == MR_EVENT) {
        	mr_event = dynamic_cast<MakeRunEvent *>(*it);
			assert(mr_event);
            peer_tid = mr_event->get_peer_tid();
#ifdef DEBUG_GROUP
            mtx.lock();
            if (who_make_me_run_map.find(peer_tid) != who_make_me_run_map.end()) {
                LOG_S(WARNING) << "Warn: multiple make runnable " << std::fixed << std::setprecision(1) \
                    << mr_event->get_abstime() << std::endl\
               		<< "\tPrevious make runnalbe at " << std::fixed << std::setprecision(1) \
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
                LOG_S(INFO) << "skip wait event at " \
                    << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                mtx.unlock();
#endif
                continue;
            } else if ((*rit)->get_op() == "MSC_mach_reply_port") {
#if DEBUG_GROUP
                mtx.lock();
                LOG_S(INFO) << "remove event at "\
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
            LOG_S(INFO) << "remove event " << i << " at "\
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
                    LOG_S(INFO) << "no matching mr event " << i << " at "\
                         << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                    mtx.unlock();
                }
            } else {
                mtx.lock();
                LOG_S(INFO) << "no matching event " << i << " at "\
                        << std::fixed << std::setprecision(1) << (*rit)->get_abstime() << std::endl;
                mtx.unlock();
#endif
            }
            return remove_count;
        }
    }
    return remove_count;
}

ProcessInfo *ListsCategory::get_process_info(tid_t tid, EventBase *back)
{
	//tpc_maps are pre-allocated in prepare_list_for_tid()

    assert(LoadData::tpc_maps.find(tid) != LoadData::tpc_maps.end());
	if (LoadData::tpc_maps[tid] == nullptr) {
		ProcessInfo *ret = new ProcessInfo(tid, tid2pid(tid), back->get_procname());
		LoadData::tpc_maps[tid] = ret;
    } 
    assert(LoadData::tpc_maps[tid] != nullptr);

	return LoadData::tpc_maps[tid];
}

void ListsCategory::update_events_in_thread(uint64_t tid)
{
    event_list_t &tid_list = tid_lists[tid]; 
	if (tid_list.size() == 0) {
		LOG_S(INFO) << "thread list with size 0" << std::endl;
		return;
	}
    event_list_t::iterator it;
    EventBase *event, *prev = nullptr;
    ProcessInfo *p = get_process_info(tid, tid_list.back());
    NSAppEventEvent *appevent, *appevent_begin = nullptr;

    for (it = tid_list.begin(); it != tid_list.end(); it++) {
        event = *it;
        if (p)
            event->update_process_info(*p);
    
        event->set_event_prev(prev);
        prev = event;

        /* update nsappevent of begin which does not record detail*/
        if (event->get_event_type() == NSAPPEVENT_EVENT) {
            appevent = dynamic_cast<NSAppEventEvent*>(event);
            if (appevent->is_begin()) {
                appevent_begin = appevent;
            } else if (appevent_begin != nullptr) {
                appevent_begin->set_event(appevent->get_event_class(), 
					appevent->get_key_code());
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
    LOG_S(INFO) << __func__ << " update_event thread " << std::hex << tid << " size " << tid_list.size() << std::endl;
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

bool ThreadType::is_runloop_thread(tid_t tid)
{
	if (categories.find(RunLoopThreadType) != categories.end()
		&& categories[RunLoopThreadType].find(tid) != categories[RunLoopThreadType].end())
		return true;
	return false;
}

void ThreadType::check_host_uithreads(event_list_t &backtrace_list)
{
	if (LoadData::meta_data.host == "Undefined") {
		mtx.lock();
		LOG_S(INFO) << "Host is not set." << std::endl;
		mtx.unlock();
		return;
	}

    std::string mainthread("[NSApplication run]");
    std::string mainthreadobserver("MainLoopObserver");
    std::string mainthreadsendevent("SendEventToEventTargetWithOptions");
    std::string nsthread("_NSEventThread");

	for (auto event : backtrace_list) {
		if (main_thread != -1 && nsevent_thread != -1)
			break;
			
        tid_t tid = event->get_tid();

        if (event->get_procname() != LoadData::meta_data.host
            || tid == main_thread
            || tid == nsevent_thread)
            continue;

		BacktraceEvent *backtrace_event = dynamic_cast<BacktraceEvent *>(event);
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
    mtx.lock();
    LOG_S(INFO) << "Mainthread " << std::hex << main_thread << std::endl;
    LOG_S(INFO) << "Eventthread " << std::hex << nsevent_thread << std::endl;
    mtx.unlock();
}


/////////////////////////////////////////
Groups::Groups(EventLists *eventlists_ptr)
:ListsCategory(eventlists_ptr->get_event_lists()), ThreadType()
{
    groups.clear();
    main_groups.clear();
    host_groups.clear();
    sub_results.clear();

	
	for (auto element : tid_lists) {
        std::map<uint64_t, Group *> temp;
        temp.clear();
        sub_results[element.first] = temp;
    }

    init_groups();
}

Groups::Groups(op_events_t &_op_lists)
:ListsCategory(_op_lists), ThreadType()
{
    groups.clear();
    main_groups.clear();
    host_groups.clear();
    sub_results.clear();

	for (auto element : tid_lists) {
        std::map<uint64_t, Group *> temp;
        temp.clear();
        sub_results[element.first] = temp;
    }

    init_groups();
}

Groups::Groups(Groups &copy_groups)
:ListsCategory(copy_groups.get_op_lists()), ThreadType()
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
    Group *cur_group;
	for (auto element : groups) {
        cur_group = element.second;//it->second;
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

    time_t time_init, time_begin, time_check;
	time(&time_init);
    LOG_S(INFO) << "Begin init group..." << std::endl;
    time(&time_begin);
    /*1. divide event list */
    para_preprocessing_tidlists();
    para_identify_thread_types();

	time(&time_check);
    LOG_S(INFO) << "para processing costs " << std::fixed << std::setprecision(1)\
			  << difftime(time_check, time_begin) << " seconds" << std::endl;
	

    /*2. para-processing connector peers */
    time(&time_begin);
    para_connector_generate();
	time(&time_check);
    LOG_S(INFO) << "para connect costs " << std::fixed << std::setprecision(1)\
			 << difftime(time_check, time_begin) << " seconds" << std::endl;

    /*3. para-grouping */
    time(&time_begin);
	para_thread_divide();
	time(&time_check);

    LOG_S(INFO) << "para group costs " << std::fixed << std::setprecision(1)\
			 << difftime(time_check, time_begin) << " seconds" << std::endl;
    LOG_S(INFO) << "init group costs " << std::fixed << std::setprecision(1)\
			 << difftime(time_check, time_init) << " seconds in total" << std::endl;
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

    ioService.post(boost::bind(&ThreadType::check_host_uithreads, this,
				get_list_of_op(BACKTRACE)));

    ioService.post(boost::bind(&ThreadType::check_rlthreads, this,
				get_list_of_op(RL_BOUNDARY),
				get_list_of_op(RL_OBSERVER)));
    
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
	ioService.post(boost::bind(&CoreAnimationConnection::core_animation_connection,
				core_animation_connection ));

	/*match breakpoint_trap connection*/
	BreakpointTrapConnection breakpoint_trap_connection(get_list_of_op(BREAKPOINT_TRAP));
	ioService.post(boost::bind(&BreakpointTrapConnection::breakpoint_trap_connection,
				breakpoint_trap_connection));

	/*match rl work*/
	RunLoopTaskConnection runloop_connection(get_list_of_op(RL_BOUNDARY), tid_lists);
	ioService.post(boost::bind(&RunLoopTaskConnection::runloop_connection, runloop_connection));

	/*fill wait -- makerun info*/
	MakeRunnableWaitPair mkrun_wait_pair(get_list_of_op(MACH_WAIT),
			get_list_of_op(MACH_MK_RUN));
	ioService.post(boost::bind(&MakeRunnableWaitPair::pair_wait_mkrun,  mkrun_wait_pair));

	/*fill voucher info*/
	VoucherBankAttrs voucher_bank_attrs(get_list_of_op(MACH_BANK_ACCOUNT),
			get_list_of_op(MACH_IPC_VOUCHER_INFO));
	ioService.post(boost::bind(&VoucherBankAttrs::update_event_info,
				voucher_bank_attrs));

    work.reset();
    threadpool.join_all();
    //ioService.stop();
}

void Groups::para_thread_divide(void)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));

    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    //uint64_t idx = 0, main_idx = -1;
    std::vector<uint64_t> host_idxes;
    host_idxes.clear();

	for (auto element : tid_lists) {
        if ((element.second).size() == 0)
            continue;
        if (LoadData::pid2comm(tid2pid(element.first)) == LoadData::meta_data.host)
            host_idxes.push_back(element.first);

		if (is_runloop_thread(element.first)) {
            RunLoopThreadDivider rl_thread_divider(element.first, sub_results, element.second, false);
            //rl_thread_divider.divide();
            ioService.post(boost::bind(&RunLoopThreadDivider::divide, rl_thread_divider));
		} else {
            ThreadDivider general_thread_divider(element.first, sub_results, element.second);
            //general_thread_divider.divide();
            ioService.post(boost::bind(&ThreadDivider::divide, general_thread_divider));
		}
    }
    
    work.reset();
    threadpool.join_all();
    //ioService.stop();
    
	for (auto element : sub_results)
		collect_groups(element.second);
    
	for (auto idx : host_idxes) 
		host_groups.insert(sub_results[idx].begin(), sub_results[idx].end());

	if (sub_results.find(main_thread) != sub_results.end())
		main_groups = sub_results[main_thread];
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
    LOG_S(INFO) << "Try to detect spinning group" << std::endl;
    if (nsevent_thread == -1) {
        LOG_S(INFO) << "NSEvent thread is not identified" << std::endl;
        return nullptr;
    }

    double nsthread_spin_timestamp  = 0.0;
    event_list_t backtrace_events = get_list_of_op(BACKTRACE);
    //event_list_t::reverse_iterator rit;
    //for (rit = backtrace_events.rbegin(); rit != backtrace_events.rend(); rit++) {
    for (auto event : backtrace_events) {
		//if ((*rit)->get_tid() != nsevent_thread)
        if (event->get_tid() != nsevent_thread)
			continue;
        //BacktraceEvent *backtrace = dynamic_cast<BacktraceEvent *>(*rit);
        BacktraceEvent *backtrace = dynamic_cast<BacktraceEvent *>(event);
        if (backtrace->spinning() == true) {
            LOG_S(INFO) << "Spindetect: " << std::hex << backtrace->get_group_id() << " at " \
                << std::fixed << std::setprecision(1) << event->get_abstime() << std::endl;
            nsthread_spin_timestamp = backtrace->get_abstime();
        }
    }
    
    if (nsthread_spin_timestamp > 0.0) {
        std::map<uint64_t, Group *>mainthread_groups = sub_results[main_thread];
        std::map<uint64_t, Group *>::reverse_iterator rit;

        LOG_S(INFO) << "Spinning detected at " << std::fixed << std::setprecision(1)\
		    << nsthread_spin_timestamp << std::endl;
		LOG_S(INFO) << "Main thread has " << mainthread_groups.size() << " groups" << std::endl;
        
        for (rit = mainthread_groups.rbegin(); rit != mainthread_groups.rend(); rit++) {
            Group *cur_group = rit->second;
            if (cur_group->get_first_event()->get_abstime() < nsthread_spin_timestamp) {
                LOG_S(INFO) << "Corresponding busy UI execution: Group #" \
					<< std::hex << cur_group->get_group_id() << std::endl;
                return cur_group;
            }
        }
    }

    LOG_S(INFO) << "Spinning is not detected" << std::endl;
    return nullptr;
}

bool Groups::matched(Group *target)
{
#if DEBUG_GROUP
    mtx.lock();
    LOG_S(INFO) << "match group " << std::hex <<  target->get_group_id() << std::endl;
    mtx.unlock();
#endif
    std::map<uint64_t, Group *>::iterator it;

    for (it = groups.begin(); it != groups.end(); it++) {
        if (*(it->second) == *target) {
#if DEBUG_GROUP
            mtx.lock();
            LOG_S(INFO) << "group " << std::hex << target->get_group_id() << " matched" << std::endl;
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
    LOG_S(INFO) << "Check interleaved msg receive at: " << std::fixed << std::setprecision(1)\
			 << msg_event->get_abstime() << std::endl;
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
            LOG_S(INFO) << "Interleaved procs: " << msg_event->get_procname() \
                << "\t" << other->get_peer()->get_procname() << " at " \
                << std::fixed << std::setprecision(1) << other->get_abstime() \
                << "\t" << msg_event->get_peer()->get_procname() << std::endl;
            mtx.unlock();
            found = true;
        }
    }
    if (found == true) {
        mtx.lock();
        LOG_S(INFO) << "Interleaved patterns for " << msg_event->get_procname();
        LOG_S(INFO) << "\treceive: "<< std::fixed << std::setprecision(1) << msg_event->get_abstime();
        LOG_S(INFO) << "\treply: " << std::fixed << std::setprecision(1)
				<< msg_event->get_next()->get_abstime() << std::endl;
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
    LOG_S(INFO) << "Checking mach message interleave inside execution segment" << std::endl;
    for (it = tid_lists.begin(); it != tid_lists.end(); it++) {
        if ((it->second).size() == 0)
            continue;
        ioService.post(boost::bind(&Groups::check_interleavemsg_from_thread, this, it->second));
    }
    work.reset();
    threadpool.join_all();
}
