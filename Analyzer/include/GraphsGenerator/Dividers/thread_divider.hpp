#ifndef THREAD_DIVIDER_HPP
#define THREAD_DIVIDER_HPP
#include "group.hpp"
#include <stack>

#define THD_BACKTRACE   1
#define THD_TIMERCREATE 2
#define THD_TIMERCANCEL 3
#define THD_TIMERCALL    4

class ThreadDivider {
public:
    uint32_t msg_induced_node;
    uint32_t wait_block_disp_node;
protected:
    uint64_t gid_base;
    Group *cur_group;
    std::map<uint64_t, Group *> ret_map;

    IntrEvent *potential_root;
    BacktraceEvent *backtrace_for_hook;
    VoucherEvent *voucher_for_hook;
    SyscallEvent *syscall_event;
    SyscallEvent *pending_msg_sent;

    BlockDequeueEvent *dequeue_event;

    std::stack<DispatchQueueMIGServiceEvent *> dispatch_mig_servers;
    std::stack<BlockInvokeEvent *> current_disp_invokers;

    FakedWokenEvent *faked_wake_event;

    std::list<EventBase *> tid_list;
    uint64_t index;
    std::map<uint64_t,std::map<uint64_t, Group*> > &submit_result;

public:
    ThreadDivider(int index,std::map<uint64_t,std::map<uint64_t, Group*> >&sub_results, std::list<EventBase *> ev_list);

    Group *create_group(uint64_t id, EventBase *root_event);
    Group *gid2group(uint64_t gid);
    void delete_group(Group *del_group);

    void add_general_event_to_group(EventBase *event);
    void store_event_to_group_handler(EventBase *event);

    void add_tsm_event_to_group(EventBase *event);
    void add_mr_event_to_group(EventBase *event);
    bool matching_wait_syscall(WaitEvent *wait);
    void add_wait_event_to_group(EventBase *event);
    void add_timercallout_event_to_group(EventBase *event);
    void add_disp_invoke_begin_event(BlockInvokeEvent *invoke_event);
    void add_disp_invoke_end_event(BlockInvokeEvent *invoke_event, BlockInvokeEvent *begin_invoke);
    void add_disp_invoke_event_to_group(EventBase *event);
    void check_hooks(MsgEvent *msg_event);
    void add_events(MsgEvent *msg_event, bool need_calculate);
    void add_msg_event_into_group(EventBase *event);
    void add_hwbr_event_into_group(EventBase *event);
    void add_disp_mig_event_to_group(EventBase *event);
    std::set<std::string> calculate_msg_peer_set(MsgEvent *, VoucherEvent *);

    void decode_groups(std::map<uint64_t, Group *> &uievent_groups, std::string filepath);
    void streamout_groups(std::map<uint64_t, Group *>& uievent_groups, std::string filepath);
    virtual void divide();
};

class RunLoopThreadDivider: public ThreadDivider {
    bool no_entry_observer;
    Group *save_cur_rl_group_for_invoke;
    BlockInvokeEvent *invoke_in_rl;
public:
    RunLoopThreadDivider(int index,std::map<uint64_t,std::map<uint64_t, Group *> >&sub_results,
		std::list<EventBase *> ev_list, bool no_observer_entry);
    ~RunLoopThreadDivider();
    void add_observer_event_to_group(EventBase *event);
    void add_nsappevent_event_to_group(EventBase *event);
    void add_rlboundary_event_to_group(EventBase *event);
	
    //void add_disp_invoke_event_to_group(EventBase *event);
    //void add_msg_event_into_group(EventBase *event);

    void decode_groups(std::string filepath) {ThreadDivider::decode_groups(ret_map, filepath);}
    void streamout_groups(std::string filepath) {ThreadDivider::streamout_groups(ret_map, filepath);}
    virtual void divide();
};

#endif
