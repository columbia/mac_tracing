#include "thread_divider.hpp"

static string _applicationMain("NSApplicationMain");
static string _nextEventMatchingMask("NSApplication _nextEventMatchingEventMask");
static string _pre_convert_event("pre_convert_event");
static string _CARender("CA::");
static string _Convert1CGEvent("Convert1CGEvent(unsigned char)");
static string _post_convert_event("post_convert_event");
static string _disableSuddenTermination("NSApplication _disableSuddenTermination");
static string _sendEvent("NSApplication sendEvent");
static string _enableSuddenTermination("_HIEnableSuddenTerminationForSendEvent");
static string _None("_None");
static string _Init("_Init");

MainThreadDivider::MainThreadDivider(int _index, vector<map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list)
:submit_result(sub_results)
{
	main_thread_ev_list = ev_list;
	state = _Init;
	index = _index;
	gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
	cerr << "main thread id " << hex << ev_list.front()->get_tid() << endl;
	cerr << "last event in main thread " << fixed << setprecision(1) << ev_list.back()->get_abstime() << endl;
}

MainThreadDivider::~MainThreadDivider(void)
{
	uievent_groups.clear();
}

string MainThreadDivider::get_state(backtrace_ev_t * backtrace_event)
{
	if (backtrace_event->check_backtrace_symbol(_CARender))
		return _CARender;

	if (!backtrace_event->check_backtrace_symbol(_applicationMain))
		return _None;

	if (backtrace_event->check_backtrace_symbol(_sendEvent))
		return _sendEvent;

	if (backtrace_event->check_backtrace_symbol(_nextEventMatchingMask)) {
		if (state == _enableSuddenTermination || state == _pre_convert_event || state == _CARender || state == _Init) {
			if (backtrace_event->check_backtrace_symbol(_Convert1CGEvent))
				return  _Convert1CGEvent;
			return _pre_convert_event;
		}
		//else  _Convert1CGEvent, disable, send_event, post_convert_event
		return _post_convert_event;
		//return _nextEventMatchingMask;
	}

	if (backtrace_event->check_backtrace_symbol(_disableSuddenTermination))
		return _disableSuddenTermination;

	if (backtrace_event->check_backtrace_symbol(_enableSuddenTermination))
		return _enableSuddenTermination;

	return _None;
}

void MainThreadDivider::divide()
{
	list<event_t *>::iterator it;
	group_t * cur_group = NULL;

	event_t * event;
	backtrace_ev_t * backtrace_for_hook = NULL;
	voucher_ev_t * voucher_for_hook = NULL;
	string cur_event_state;

	double count_begin = main_thread_ev_list.front()->get_abstime();

	cerr << "main thread events " << hex << main_thread_ev_list.size() << endl;
	for (it = main_thread_ev_list.begin(); it != main_thread_ev_list.end(); it++) {
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

		/* grouping */
		if (backtrace_event == NULL) {
			cur_group->add_to_container(event);
			continue;
		}
		
		cur_event_state = get_state(backtrace_event);
		if (cur_event_state == _None) {
			cur_group->add_to_container(event);
			continue;
		}
		
		cerr << state << "->" <<  cur_event_state << "\tat\t" << fixed << setprecision(1) << event->get_abstime() << endl;

		/* recognize ui event transactions via backtrace */

		/*first, aggregate time*/
		cur_group->attribute_time(state, event->get_abstime() - count_begin);
		count_begin = event->get_abstime();

		/*second, state change*/
		string prev_state = state;

		if (state == _Init || state == _pre_convert_event) {
			if (cur_event_state == _Convert1CGEvent)
				state = _post_convert_event;
			else
				state = cur_event_state;
			cur_group->add_to_container(event);
			if (state != prev_state) {
				cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}
		
		else if (state == _CARender) {
			if (cur_event_state == _pre_convert_event || cur_event_state == _Convert1CGEvent) {
				cur_group->set_state_end(state, event);
				prev_state = _None;
				cur_group = create_group(gid_base + uievent_groups.size(), NULL);
				uievent_groups[cur_group->get_group_id()] = cur_group;
			}
			state = cur_event_state;

			if (cur_event_state == _Convert1CGEvent) {
				state = _post_convert_event;
			}
			cur_group->add_to_container(event);

			if (state != prev_state) {
				if (prev_state != _None)
					cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}

		else if (state == _post_convert_event) {
			if (cur_event_state == _CARender)
				state = _post_convert_event;
			else
				state = cur_event_state;
			cur_group->add_to_container(event);
			if (state != prev_state) {
				cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}

		else if (state == _disableSuddenTermination) {
			state = cur_event_state;
			cur_group->add_to_container(event);
			if (state != prev_state) {
				cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}
		
		else if (state == _sendEvent) {
			if (cur_event_state != _enableSuddenTermination) {
				state = _sendEvent;
			} else {
				state = _enableSuddenTermination;
			}
			cur_group->add_to_container(event);
			if (state != prev_state) {
				cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}

		else if (state == _enableSuddenTermination) {
			if (cur_event_state == _Convert1CGEvent || cur_event_state == _pre_convert_event || cur_event_state == _CARender) {
				cur_group->set_state_end(state, event);
				prev_state = _None;
				cur_group = create_group(gid_base + uievent_groups.size(), NULL);
				uievent_groups[cur_group->get_group_id()] = cur_group;
			}
			state = cur_event_state;
			if (cur_event_state == _Convert1CGEvent)
				state = _post_convert_event;
			cur_group->add_to_container(event);

			if (state != prev_state) {
				if (prev_state != _None)
					cur_group->set_state_end(prev_state, event);
				cur_group->set_state_begin(state, event);
			}
		}

		else {
			cerr << "Unkown state " << state << endl;
		}

		cerr << "Current state " << state << endl;
	}
	submit_result[index] = uievent_groups;
}
