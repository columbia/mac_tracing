#include "timercall_pattern.hpp"
#include "eventlistop.hpp"

#define TIMERCALL_DEBUG 0

TimercallPattern::TimercallPattern(list<event_t*> &_timercreate_list, list<event_t *> &_timercallout_list, list<event_t*> &_timercancel_list)
:timercreate_list(_timercreate_list), timercallout_list(_timercallout_list), timercancel_list(_timercancel_list)
{
}

void TimercallPattern::connect_timercall_patterns(void)
{
#ifdef TIMERCALL_DEBUG
	cerr << "begin matching timercall pattern ... " << endl;
#endif
	connect_create_and_cancel();
	connect_create_and_timercallout();
#ifdef TIMERCALL_DEBUG
	cerr << "finish matching timercall pattern. " << endl;
#endif
}

void TimercallPattern::connect_create_and_cancel(void)
{
	list<event_t *>::iterator it;
	list<event_t *> mix_list;
	list<timercreate_ev_t*> tmp_create_list;
	timercreate_ev_t * timercreate_event;
	timercancel_ev_t * timercancel_event;

	mix_list.insert(mix_list.end(), timercreate_list.begin(), timercreate_list.end());
	mix_list.insert(mix_list.end(), timercancel_list.begin(), timercancel_list.end());
	EventListOp::sort_event_list(mix_list);

	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		timercreate_event = dynamic_cast<timercreate_ev_t *>(*it);
		if (timercreate_event) {
			tmp_create_list.push_back(timercreate_event);
		} else {
			timercancel_event = dynamic_cast<timercancel_ev_t *>(*it);
			connect_timercreate_for_timercancel(tmp_create_list, timercancel_event);
		}
	}
	//TODO : check the rest events
}

bool TimercallPattern::connect_timercreate_for_timercancel(list<timercreate_ev_t *>&tmp_create_list, timercancel_ev_t *timercancel_event)
{
	list<timercreate_ev_t *>::reverse_iterator rit;
	for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
		timercreate_ev_t * timercreate_event = dynamic_cast<timercreate_ev_t *>(*rit);
		if (timercancel_event->check_root(timercreate_event) == true) {
			timercancel_event->set_timercreate(timercreate_event);
			timercreate_event->cancel_call(timercancel_event);
			tmp_create_list.erase(next(rit).base());
			return true;
		}
	}
#if TIMERCALL_DEBUG
	cerr << "Warn: no timercreate for timercancel " << fixed << setprecision(1) << timercancel_event->get_abstime() << endl; 
#endif
	return false;
}

void TimercallPattern::connect_create_and_timercallout(void)
{
	list<event_t *>::iterator it;
	list<event_t *> mix_list;
	list<timercreate_ev_t*> tmp_create_list;
	timercreate_ev_t * timercreate_event;
	timercallout_ev_t * timercallout_event;

	mix_list.insert(mix_list.end(), timercreate_list.begin(), timercreate_list.end());
	mix_list.insert(mix_list.end(), timercallout_list.begin(), timercallout_list.end());
	EventListOp::sort_event_list(mix_list);
	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		timercreate_event = dynamic_cast<timercreate_ev_t *>(*it);
		if (timercreate_event) {
			tmp_create_list.push_back(timercreate_event);
		} else {
			timercallout_event = dynamic_cast<timercallout_ev_t*>(*it);
			connect_timercreate_for_timercallout(tmp_create_list, timercallout_event);
		}
	}
}

bool TimercallPattern::connect_timercreate_for_timercallout(list<timercreate_ev_t*> &tmp_create_list, timercallout_ev_t *timercallout_event)
{
	list<timercreate_ev_t *>::reverse_iterator rit;
	for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
		timercreate_ev_t * timercreate_event = dynamic_cast<timercreate_ev_t *>(*rit);
		if (timercallout_event->check_root(timercreate_event) == true) {
			timercallout_event->set_timercreate(timercreate_event);
			timercreate_event->set_called_peer(timercallout_event);
			tmp_create_list.erase(next(rit).base());
			return true;
		}
	}

#if TIMERCALL_DEBUG
	cerr << "Warn: no timercreate for timercallout " << fixed << setprecision(1) << timercallout_event->get_abstime() << endl; 
#endif
	return false;
}
