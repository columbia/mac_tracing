#ifndef THREAD_DIVIDER_HPP
#define THREAD_DIVIDER_HPP
#include "group.hpp"
#include <stack>

#define THD_BACKTRACE   1
#define THD_TIMERCREATE 2
#define THD_TIMERCANCEL 3
#define THD_TIMERCALL	4

class ThreadDivider {
protected:
	uint64_t gid_base;
	group_t *cur_group;
	map<uint64_t, group_t *> ret_map;

	intr_ev_t *potential_root;
	backtrace_ev_t *backtrace_for_hook;
	voucher_ev_t *voucher_for_hook;
	syscall_ev_t *syscall_event;
	syscall_ev_t *pending_msg_sent;

	dequeue_ev_t *dequeue_event;
	//blockinvoke_ev_t *mig_server_invoker;

	stack<disp_mig_ev_t *> dispatch_mig_servers;
	stack<blockinvoke_ev_t *> current_disp_invokers;

	fakedwoken_ev_t *faked_wake_event;

	list<event_t *> tid_list;
	uint64_t index;
	map<uint64_t, map<uint64_t, group_t*> > &submit_result;

public:
	ThreadDivider(int index, map<uint64_t, map<uint64_t, group_t*> >&sub_results, list<event_t *> ev_list);

	group_t *create_group(uint64_t id, event_t *root_event);
	group_t *gid2group(uint64_t gid);
	void delete_group(group_t *del_group);

	void add_general_event_to_group(event_t *event);
	void store_event_to_group_handler(event_t *event);

	void add_tsm_event_to_group(event_t *event);
	void add_mr_event_to_group(event_t *event);
	bool matching_wait_syscall(wait_ev_t *wait);
	void add_wait_event_to_group(event_t *event);
	void add_timercallout_event_to_group(event_t *event);
	void add_disp_invoke_event_to_group(event_t *event);
	bool check_group_with_voucher(voucher_ev_t *voucher, group_t *cur_group, pid_t msg_peer);
	void add_msg_event_into_group(event_t *event);
	void add_hwbr_event_into_group(event_t *event);
	void add_disp_mig_event_to_group(event_t *event);

	void decode_groups(map<uint64_t, group_t *> &uievent_groups, string filepath);
	void streamout_groups(map<uint64_t, group_t *>& uievent_groups, string filepath);
	virtual void divide();
};

class RLThreadDivider: public ThreadDivider {
	bool no_entry_observer;
	group_t *save_cur_rl_group_for_invoke;
	blockinvoke_ev_t *invoke_in_rl;
public:
	RLThreadDivider(int index, map<uint64_t, map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list, bool no_observer_entry);
	~RLThreadDivider();
	void add_observer_event_to_group(event_t *event);
	void add_nsappevent_event_to_group(event_t *event);
	void add_rlboundary_event_to_group(event_t *event);
	void add_disp_invoke_event_to_group(event_t *event);
	void add_msg_event_into_group(event_t *event);

	void decode_groups(string filepath) {ThreadDivider::decode_groups(ret_map, filepath);}
	void streamout_groups(string filepath) {ThreadDivider::streamout_groups(ret_map, filepath);}
	virtual void divide();
};

class WQThreadDivider: public ThreadDivider {
	
public:
	WQThreadDivider(int index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list);
	void decode_groups(string filepath) {ThreadDivider::decode_groups(ret_map, filepath);}
	void streamout_groups(string filepath) {ThreadDivider::streamout_groups(ret_map, filepath);}
	virtual void divide();
}; 

#endif
