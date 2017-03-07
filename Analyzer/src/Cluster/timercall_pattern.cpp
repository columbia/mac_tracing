#include "timercall_pattern.hpp"
#include "eventlistop.hpp"

//#define  TIMERCALL_DEBUG

TimercallPattern::TimercallPattern(list<event_t*> &_callcreate_list, list<event_t *> &_callout_list, list<event_t*> &_callcancel_list)
:callcreate_list(_callcreate_list), callout_list(_callout_list), callcancel_list(_callcancel_list)
{
}

void TimercallPattern::connect_create_and_cancel(void)
{
	list<event_t *>::iterator it;
	list<event_t *> mix_list;
	list<callcreate_ev_t*> tmp_create_list;
	callcreate_ev_t * callcreate_event;
	callcancel_ev_t * callcancel_event;

	mix_list.insert(mix_list.end(), callcreate_list.begin(), callcreate_list.end());
	mix_list.insert(mix_list.end(), callcancel_list.begin(), callcancel_list.end());
	EventListOp::sort_event_list(mix_list);

	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		callcreate_event = dynamic_cast<callcreate_ev_t *>(*it);
		if (callcreate_event) {
			tmp_create_list.push_back(callcreate_event);
		} else {
			callcancel_event = dynamic_cast<callcancel_ev_t *>(*it);
			connect_callcreate_for_callcancel(tmp_create_list, callcancel_event);
		}
	}
	//TODO : check the rest events
}

bool TimercallPattern::connect_callcreate_for_callcancel(list<callcreate_ev_t *>&tmp_create_list, callcancel_ev_t *callcancel_event)
{
	list<callcreate_ev_t *>::reverse_iterator rit;
	for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
		callcreate_ev_t * callcreate_event = dynamic_cast<callcreate_ev_t *>(*rit);
		if (callcancel_event->check_root(callcreate_event) == true) {
			callcancel_event->set_callcreate(callcreate_event);
			callcreate_event->cancel_call(callcancel_event);
			tmp_create_list.erase(next(rit).base());
			return true;
		}
	}
	#ifdef TIMERCALL_DEBUG
	cerr << "Warn: no callcreate for callcancel " << fixed << setprecision(1) << callcancel_event->get_abstime() << endl; 
	#endif
	return false;
}

void TimercallPattern::connect_create_and_callout(void)
{
	list<event_t *>::iterator it;
	list<event_t *> mix_list;
	list<callcreate_ev_t*> tmp_create_list;
	callcreate_ev_t * callcreate_event;
	callout_ev_t * callout_event;

	mix_list.insert(mix_list.end(), callcreate_list.begin(), callcreate_list.end());
	mix_list.insert(mix_list.end(), callout_list.begin(), callout_list.end());
	EventListOp::sort_event_list(mix_list);
	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		callcreate_event = dynamic_cast<callcreate_ev_t *>(*it);
		if (callcreate_event) {
			tmp_create_list.push_back(callcreate_event);
		} else {
			callout_event = dynamic_cast<callout_ev_t*>(*it);
			connect_callcreate_for_callout(tmp_create_list, callout_event);
		}
	}
}

bool TimercallPattern::connect_callcreate_for_callout(list<callcreate_ev_t*> &tmp_create_list, callout_ev_t *callout_event)
{
	list<callcreate_ev_t *>::reverse_iterator rit;
	for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
		callcreate_ev_t * callcreate_event = dynamic_cast<callcreate_ev_t *>(*rit);
		if (callout_event->check_root(callcreate_event) == true) {
			callout_event->set_callcreate(callcreate_event);
			callcreate_event->set_called_peer(callout_event);
			tmp_create_list.erase(next(rit).base());
			return true;
		}
	}

	#ifdef TIMERCALL_DEBUG
	cerr << "Warn: no callcreate for callout " << fixed << setprecision(1) << callout_event->get_abstime() << endl; 
	#endif
	return false;
}
