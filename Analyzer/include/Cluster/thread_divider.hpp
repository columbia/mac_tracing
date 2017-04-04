#ifndef THREAD_DIVIDER_HPP
#define THREAD_DIVIDER_HPP
#include "group.hpp"

#define THD_BACKTRACE   1
#define THD_TIMERCREATE 2
#define THD_TIMERCANCEL 3
#define THD_TIMERCALL	4

class ThreadDivider {
public:
	ThreadDivider();
	group_t * create_group(uint64_t id, event_t * root_event);
	void decode_groups(map<uint64_t, group_t *> & uievent_groups, string filepath);
	void streamout_groups(map<uint64_t, group_t *> & uievent_groups, string filepath);
	virtual void divide() {}
};

class MainThreadDivider : public ThreadDivider{
	list<event_t *> main_thread_ev_list;
	uint64_t gid_base;
	map<uint64_t, group_t *> uievent_groups;
	int index;
	vector<map<uint64_t, group_t *> > &submit_result;
	string state;
	string get_state(backtrace_ev_t * backtrace_event);

public:
	MainThreadDivider(int index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list);
	~MainThreadDivider();
	void decode_groups(string filepath) {ThreadDivider::decode_groups(uievent_groups, filepath);}
	void streamout_groups(string filepath) {ThreadDivider::streamout_groups(uievent_groups, filepath);}
	void divide();
};

//static string _cfRunLoopServiceMachPort("CFRunLoopServiceMachPort");
//static string _cfRunLoopDoSource1("__CFRunLoopDoSource1");
//static string _cancelTimer("cancelTimer");
//static string _reArmTimer("reArmTimer");
//static string _createTimer("createTimer");
//static string _pullEventFromWindowServer("PullEventsFromWindowServerOnConnection(unsigned int, unsigned char, __CFMachPortBoost*)");
//static string _cfRunLoopWakeUp("CFRunLoopWakeUp");
//static string _None("_None");
//static string _Init("_Init");

class NSEventThreadDivider : public ThreadDivider {
	list<event_t *> nsevent_thread_ev_list;
	uint64_t gid_base;
	map<uint64_t, group_t *> uievent_groups;
	int index;
	vector<map<uint64_t, group_t *> > &submit_result;

	//uint64_t group_num;
	string state;
	struct {
		void * func_ptr;
		uint64_t param0;
	} spinning_timer;
	typedef struct {
		backtrace_ev_t * backtrace_event;
		timercreate_ev_t * timercreate_event;
		timercancel_ev_t * timercancel_event;
		timercallout_ev_t * timercallout_event;
	} ptrs_t;

	uint32_t update_ptrs(ptrs_t *ptrs, event_t * event);
	void set_spinning_timer_func();
	string get_state(backtrace_ev_t * backtrace_event);
	string get_state(timercreate_ev_t *);
	string get_state(timercancel_ev_t *);
	string get_state(timercallout_ev_t *);
	bool create_timer(timercreate_ev_t *);
	bool cancel_timer(timercancel_ev_t *);
	bool callout_timer(timercallout_ev_t *);

	group_t * add_backtrace_to_group(backtrace_ev_t * backtrace_event, group_t * cur_group);
	group_t * add_timercreate_to_group(timercreate_ev_t * timercreate_event, group_t * cur_group);
	group_t * add_timercancel_to_group(timercancel_ev_t * timercancel_event, group_t * cur_group);
	group_t * add_timercallout_to_group(timercallout_ev_t * timercallout_event, group_t * cur_group);
	
public:
	NSEventThreadDivider(int index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list);
	//(list<event_t *> ev_list);
	~NSEventThreadDivider();
	void decode_groups(string filepath) {ThreadDivider::decode_groups(uievent_groups, filepath);}
	void streamout_groups(string filepath) {ThreadDivider::streamout_groups(uievent_groups, filepath);}
	void divide();
};

#endif
