#include "timercall_divider.hpp"

TimerCallDivider::TimerCallDivider(list<event_t *> & _ev_list, list<event_t *> & _bt_list, uint32_t _divide_type, event_t * _divide_event)
:ev_list(_ev_list)
{
	divide_type = _divide_type;
	event_t * _divider = get_divider(_bt_list);
	divide_event = _divider; // == NULL ? _divide_event : _divider;
	sublists.clear();
	cancel_counter = CANCELLED << SUB_BASE_SHIFT;
	fired_counter = FIRED << SUB_BASE_SHIFT;
}

timercallout_ev_t* TimerCallDivider::get_divider(list<event_t *> &bt_list)
{
	if (bt_list.size() == 0)
		return NULL;

	list<event_t*>::iterator it;
	backtrace_ev_t * backtrace_event = NULL;
	string nseventthread("_NSEventThread");

	cerr << "try to get divider " << bt_list.size() << endl;

	for (it = bt_list.begin(); it != bt_list.end(); it++) {
		backtrace_event = dynamic_cast<backtrace_ev_t *>(*it);
		if (backtrace_event && backtrace_event->check_infected()
			&& backtrace_event->check_backtrace_symbol(nseventthread)) {
			cerr << "get infected frame\n";
			break;
		}
	}

	msg_ev_t * mach_msg_event = NULL;
	if (backtrace_event != NULL) {
		it = find(ev_list.begin(), ev_list.end(), (event_t *)backtrace_event);	
		while (it != ev_list.end() && it != ev_list.begin()) {
			it--;
			if ((*it)->get_tid() == backtrace_event->get_tid()
				&& dynamic_cast<msg_ev_t *>(*it)) {
				mach_msg_event = dynamic_cast<msg_ev_t*>(*it);
				break;
			}
		}

		//mach_msg_event = dynamic_cast<msg_ev_t*>(backtrace_event->get_hooked_event());
		if (mach_msg_event) {
			mach_msg_event = mach_msg_event->get_peer();
		}
	}

	if (mach_msg_event != NULL) {
		it = find(ev_list.begin(), ev_list.end(), (event_t *)mach_msg_event);	
		while (it != ev_list.end() && it != ev_list.begin()) {
			it--;
			if ((*it)->get_tid() == mach_msg_event->get_tid() 
				&& dynamic_cast<timercallout_ev_t*>(*it)) {
				cerr << "return divider is not null" << endl;
				return dynamic_cast<timercallout_ev_t*>(*it);
			}
		}
	}
	return NULL;
}

bool TimerCallDivider::match_callevents(timercreate_ev_t* event,  timercallout_ev_t * divider)
{
	if (event->get_func_ptr() == divider->get_func_ptr()
		&& event->get_param0() == divider->get_param0())
		return true;
	return false;
}

bool TimerCallDivider::match_callevents(timercallout_ev_t* event,  timercallout_ev_t * divider)
{
	if (event->get_func_ptr() == divider->get_func_ptr()
		&& event->get_param0() == divider->get_param0())
		return true;
	return false;
}

bool TimerCallDivider::match_callevents(timercancel_ev_t* event,  timercallout_ev_t * divider)
{
	if (event->get_func_ptr() == divider->get_func_ptr()
		&& event->get_param0() == divider->get_param0())
		return true;
	return false;
}

void TimerCallDivider::divide(void)
{
	if (divide_event == NULL) {
		cerr << "No Divide Event" << endl;
		return;
	}

	list<event_t *>::iterator it;
	list<event_t *> tmp_sub;
	timercallout_ev_t * divider = dynamic_cast<timercallout_ev_t *>(divide_event);
	timercreate_ev_t * call_create;
	timercallout_ev_t * call_out;
	timercancel_ev_t * call_cancel;
	
	cerr << "try to divide" << endl;
	for (it = ev_list.begin(); it != ev_list.end(); it++) {
		call_create = dynamic_cast<timercreate_ev_t *>(*it);
		if (call_create) {
			if (match_callevents(call_create, divider))	{
				if (tmp_sub.size() > 0) {
					//sublists[counter++] = tmp_sub;
					cerr << "should not collect before timercreate " << fixed << setprecision(1) << (tmp_sub.front())->get_abstime() << endl;
					tmp_sub.clear();
				}
				tmp_sub.push_back(*it);
			}	
			continue;
		}

		call_out = dynamic_cast<timercallout_ev_t *>(*it);
		if (call_out) {
			if (match_callevents(call_out, divider)) {
				if (tmp_sub.size() > 0) {
					tmp_sub.push_back(*it);
					sublists[fired_counter++] = tmp_sub;
				}
				tmp_sub.clear();
			}
			continue;
		}

		call_cancel = dynamic_cast<timercancel_ev_t *>(*it);
		if (call_cancel) {
			if (match_callevents(call_cancel, divider)) {
				if (tmp_sub.size() > 0) {
					tmp_sub.push_back(*it);
					sublists[cancel_counter++] = tmp_sub;
				}
				tmp_sub.clear();
			}
			continue;
		}
		
		if (tmp_sub.size() > 0) {
			tmp_sub.push_back(*it);
		}
	}
}

void TimerCallDivider::compare(void)
{
	// get first two for check, can get all for normalize
	map<uint64_t, list<event_t *> >::iterator it;
	uint64_t  index;
	string filename_prefix("timer_");
	string filename;

	for (it = sublists.begin(); it != sublists.end(); it++) {
		index = it->first;
		switch (index >> SUB_BASE_SHIFT) {
			case CANCELLED:
				filename = filename_prefix + "cancelled_" + to_string(index & INDEX_MASK);
				EventListOp::streamout_all_event(sublists[index], filename.c_str());
				break;
			case FIRED:
				filename = filename_prefix + "fired_" + to_string(index & INDEX_MASK);
				EventListOp::streamout_all_event(sublists[index], filename.c_str());
				break;
			default:
				break;
		}
	}
	//TODO : compare the fired and cancelled case
}
