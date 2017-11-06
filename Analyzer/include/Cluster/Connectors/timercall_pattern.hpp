#ifndef TIMER_CALL_PATTERN_HPP
#define TIMER_CALL_PATTERN_HPP
#include "timer_callout.hpp"
class TimercallPattern {
	list<event_t *> &timercreate_list;
	list<event_t *> &timercallout_list;
	list<event_t *> &timercancel_list;
public:
	TimercallPattern(list<event_t*> &_timercreate_list, list<event_t *> &_timercallout_list, list<event_t*> &_timercancel_list);
	void connect_timercall_patterns(void);
	void connect_create_and_cancel(void);
	void connect_create_and_timercallout(void);
	bool connect_timercreate_for_timercallout(list<timercreate_ev_t*> &tmp_create_list, timercallout_ev_t *timercallout_event);
	bool connect_timercreate_for_timercancel(list<timercreate_ev_t *>&tmp_create_list, timercancel_ev_t *timercancel_event);
};
typedef TimercallPattern timercall_pattern_t;
#endif
