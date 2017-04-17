#include "thread_divider.hpp"

static string _cfRunLoopServiceMachPort("CFRunLoopServiceMachPort");
static string _cfRunLoopDoSource1("__CFRunLoopDoSource1");
static string _cancelTimer("cancelTimer");
static string _reArmTimer("reArmTimer");
static string _createTimer("createTimer");
static string _timercalloutTimer("timercalloutTimer");
static string _pullEventFromWindowServer("PullEventsFromWindowServerOnConnection(unsigned int, unsigned char, __CFMachPortBoost*)");
static string _cfRunLoopWakeUp("CFRunLoopWakeUp");
static string _None("_None");
static string _Init("_Init");

NSEventThreadDivider::NSEventThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list)
:submit_result(sub_results)
{
	nsevent_thread_ev_list = ev_list;
	state = _Init;
	index = _index;
	spinning_timer.func_ptr = 0;
	spinning_timer.param0 = 0;
	gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
}

NSEventThreadDivider::~NSEventThreadDivider()
{
	uievent_groups.clear();
}

void NSEventThreadDivider::set_spinning_timer_func()
{
	void * func_ptr = 0;
	uint64_t param0 = 0;
	list<event_t *>::iterator it;
	event_t * event;
	enum {
		init=0,
		pull_event,
		timer,
		wakeup
	} search_state = init;
	ptrs_t ptrs;

	for (it = nsevent_thread_ev_list.begin(); it != nsevent_thread_ev_list.end(); it++) {
		event = *it;
		switch(update_ptrs(&ptrs, event)) {
			case THD_BACKTRACE: {
				if (get_state(ptrs.backtrace_event) == _pullEventFromWindowServer) {//(ptrs.backtrace_event->check_backtrace_symbol(_pullEventFromWindowServer)) {
					search_state = pull_event;
				}
				else if (get_state(ptrs.backtrace_event) == _cfRunLoopWakeUp) {//(ptrs.backtrace_event->check_backtrace_symbol(_cfRunLoopWakeUp)) {
					if (search_state == timer) {
						cerr << "set spinning timer" << endl;
						spinning_timer.func_ptr = func_ptr;
						spinning_timer.param0 = param0;
					} else {
						cerr << "incorrect order " << endl;
						func_ptr = 0 ;
						param0 = 0;
						search_state = init;
					}
				}
				break;
			}
			case THD_TIMERCREATE:
				if (search_state == timer) {
					if (ptrs.timercreate_event->get_func_ptr() == func_ptr 
						&& ptrs.timercreate_event->get_param0() == param0) {
						cerr << "re-arm timer " << fixed << setprecision(1) << ptrs.timercreate_event->get_abstime() << endl;
						search_state = timer;
					}
					else {
						search_state = init;
						func_ptr = 0;
						param0 = 0;
					}
				} else if (search_state == pull_event) {
					cerr << "create timer " << fixed << setprecision(1) << ptrs.timercreate_event->get_abstime() << endl;
					func_ptr = ptrs.timercreate_event->get_func_ptr();
					param0 = ptrs.timercreate_event->get_param0();
					search_state = timer;
				}
				break;
			case THD_TIMERCANCEL:
				if (search_state == timer) {
					search_state = init;
					func_ptr = 0;
					param0 = 0;
				} else if (search_state == pull_event) {
					cerr << "cancel timer " << fixed << setprecision(1) << ptrs.timercancel_event->get_abstime() << endl;
					func_ptr = ptrs.timercancel_event->get_func_ptr();
					param0 = ptrs.timercancel_event->get_param0();
					search_state = timer;
				}
				break;

			default:
				break;
		}

		if (spinning_timer.func_ptr != 0)
			break;
	}
}

string NSEventThreadDivider::get_state(backtrace_ev_t * backtrace_event)
{
	if (backtrace_event->check_backtrace_symbol(_cfRunLoopServiceMachPort))
		return _cfRunLoopServiceMachPort;

	if (backtrace_event->check_backtrace_symbol(_cfRunLoopWakeUp))
		return _cfRunLoopWakeUp;

	if (backtrace_event->check_backtrace_symbol(_pullEventFromWindowServer))
		return _pullEventFromWindowServer;

	if (backtrace_event->check_backtrace_symbol(_cfRunLoopDoSource1))
		return _cfRunLoopDoSource1;

	return _None;
}

string NSEventThreadDivider::get_state(timercreate_ev_t * event)
{
	if (create_timer(event)) {
		/*
		if (state == _cancelTimer)
			return _reArmTimer;
		*/
		return _createTimer;
	}
	return _None;
}

string NSEventThreadDivider::get_state(timercancel_ev_t * event)
{
	if (cancel_timer(event))
		return _cancelTimer;
	return _None;
}

string NSEventThreadDivider::get_state(timercallout_ev_t * event)
{
	if (callout_timer(event))
		return _timercalloutTimer;
	return _None;
}

bool NSEventThreadDivider::create_timer(timercreate_ev_t * event)
{
	if (event->get_func_ptr() == spinning_timer.func_ptr
		&& event->get_param0() == spinning_timer.param0)
		return true;
	return false;
}

bool NSEventThreadDivider::cancel_timer(timercancel_ev_t * event)
{
	if (event->get_func_ptr() == spinning_timer.func_ptr
		&& event->get_param0() == spinning_timer.param0)
		return true;
	return false;
}

bool NSEventThreadDivider::callout_timer(timercallout_ev_t * event)
{
	if (event->get_func_ptr() == spinning_timer.func_ptr
		&& event->get_param0() == spinning_timer.param0)
		return true;
	return false;
}

uint32_t NSEventThreadDivider::update_ptrs(ptrs_t *ptrs, event_t * event)
{
	memset(ptrs, 0, sizeof(ptrs_t));
	if (dynamic_cast<backtrace_ev_t *>(event)) {
		ptrs->backtrace_event = dynamic_cast<backtrace_ev_t*>(event);
		return THD_BACKTRACE;
	}
	if (dynamic_cast<timercreate_ev_t *>(event)) {
		ptrs->timercreate_event = dynamic_cast<timercreate_ev_t *>(event);
		return THD_TIMERCREATE;
	}
	if (dynamic_cast<timercancel_ev_t *>(event)) {
		ptrs->timercancel_event = dynamic_cast<timercancel_ev_t *>(event);
		return THD_TIMERCANCEL;
	}
	if (dynamic_cast<timercallout_ev_t *>(event)) {
		ptrs->timercallout_event = dynamic_cast<timercallout_ev_t *>(event);
		return THD_TIMERCALL;
	}
	return 0;
}

group_t * NSEventThreadDivider::add_backtrace_to_group(backtrace_ev_t * backtrace_event, group_t * cur_group)
{
	string cur_event_state = get_state(backtrace_event);

	if (cur_event_state == _None) {
		cur_group->add_to_container(backtrace_event);
		cur_group->add_group_tags(backtrace_event->get_symbols());
		return cur_group;
	}
	
	if (state == _Init) {
		state = cur_event_state;
		cur_group->add_to_container(backtrace_event);
		cur_group->add_group_tags(backtrace_event->get_symbols());
		return cur_group;
	}

	if (state != _cfRunLoopServiceMachPort && cur_event_state == _cfRunLoopServiceMachPort) {
		cur_group = create_group(gid_base + uievent_groups.size(), NULL);
		uievent_groups[cur_group->get_group_id()] = cur_group;
		//if (state != _cfRunLoopWakeUp && state != _createTimer)
	}

	cur_group->add_to_container(backtrace_event);
	cur_group->add_group_tags(backtrace_event->get_symbols());
	state = cur_event_state;
	return cur_group;
}

group_t * NSEventThreadDivider::add_timercreate_to_group(timercreate_ev_t * timercreate_event, group_t * cur_group)
{
	string cur_event_state = get_state(timercreate_event);
	if (cur_event_state != _None) {
		if (state != _cancelTimer)
			cerr << "Check: Timer get fired before " << fixed << setprecision(1) <<  timercreate_event->get_abstime()  << endl;
		state = cur_event_state;
	}
	cur_group->add_to_container(timercreate_event);
	return cur_group;
}

group_t * NSEventThreadDivider::add_timercancel_to_group(timercancel_ev_t * timercancel_event, group_t * cur_group)
{
	string cur_event_state = get_state(timercancel_event);
	if (cur_event_state != _None) {
		state = cur_event_state;
	}
	cur_group->add_to_container(timercancel_event);
	return cur_group;
}

group_t * NSEventThreadDivider::add_timercallout_to_group(timercallout_ev_t * timercallout_event, group_t * cur_group)
{
	string cur_event_state = get_state(timercallout_event);
	if (cur_event_state != _None)
		state = cur_event_state;
	cur_group->add_to_container(timercallout_event);
	return cur_group;
}

void NSEventThreadDivider::divide()
{
	list<event_t *>::iterator it;
	group_t * cur_group = NULL;

	event_t * event;
	ptrs_t ptrs;

	backtrace_ev_t * backtrace_for_hook = NULL;
	voucher_ev_t * voucher_for_hook = NULL;
	
	double count_begin = nsevent_thread_ev_list.front()->get_abstime();
	
	cerr << "Set Spinning timer " << endl;
	set_spinning_timer_func();
	cerr << "Spinning timer " << hex << spinning_timer.func_ptr << "(" << hex << spinning_timer.param0 << ")"<< endl;
	
	for (it = nsevent_thread_ev_list.begin(); it != nsevent_thread_ev_list.end(); it++) {
		if (cur_group == NULL) {
			cur_group = create_group(gid_base + uievent_groups.size(), NULL);
			uievent_groups[cur_group->get_group_id()] = cur_group;
		}
		
		event = *it;
		backtrace_ev_t * backtrace_event = dynamic_cast<backtrace_ev_t*>(event);
		voucher_ev_t * voucher_event = dynamic_cast<voucher_ev_t *>(event);
		if (backtrace_event) 
			backtrace_for_hook = backtrace_event;
		
		if (voucher_event)
			voucher_for_hook = voucher_event;

		/* hook backtrace to blockinvoke event*/
		blockinvoke_ev_t * invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
		if (invoke_event) {
			if (invoke_event->is_begin() == true
				&& backtrace_for_hook != NULL
				&& backtrace_for_hook->hook_to_event(event, BLOCKINVOKE)) {
				//cur_group->add_to_container(backtrace_for_hook);
				//cur_group->add_group_tags(backtrace_for_hook->get_symbols());
				backtrace_for_hook = NULL;
			}
			cur_group->add_group_tags(invoke_event->get_desc());
		}

		/* hook voucher / backtrace to msg event */
		msg_ev_t * msg_event = dynamic_cast<msg_ev_t*>(event);
		if (msg_event) {
			if (voucher_for_hook && voucher_for_hook->hook_msg(msg_event))
				voucher_for_hook = NULL;
			if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, MSG_EVENT)) 
				backtrace_for_hook = NULL;
		}

		/*first, aggregate time*/
		cur_group->attribute_time(state, event->get_abstime() - count_begin);
		count_begin = event->get_abstime();

		/*second, state change*/
		switch(update_ptrs(&ptrs, event)) {
			case THD_BACKTRACE:
				cur_group = add_backtrace_to_group(ptrs.backtrace_event, cur_group);
				break;
			case THD_TIMERCREATE:
				cur_group = add_timercreate_to_group(ptrs.timercreate_event, cur_group);
				break;
			case THD_TIMERCANCEL:
				cur_group = add_timercancel_to_group(ptrs.timercancel_event, cur_group);
				break;
			case THD_TIMERCALL:
				cur_group = add_timercallout_to_group(ptrs.timercallout_event, cur_group);
				break;
			default:
				cur_group->add_to_container(event);
				break;
		}
	}
	submit_result[index] = uievent_groups;
}
