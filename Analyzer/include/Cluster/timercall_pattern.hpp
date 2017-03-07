#ifndef TIMER_CALL_PATTERN_HPP
#define TIMER_CALL_PATTERN_HPP
#include "timer_callout.hpp"
class TimercallPattern {
	list<event_t *> &callcreate_list;
	list<event_t *> &callout_list;
	list<event_t *> &callcancel_list;
public:
	TimercallPattern(list<event_t*> &_callcreate_list, list<event_t *> &_callout_list, list<event_t*> &_callcancel_list);
	void connect_timercall_patterns(void) {connect_create_and_cancel(); connect_create_and_callout();}
	void connect_create_and_cancel(void);
	void connect_create_and_callout(void);
	bool connect_callcreate_for_callout(list<callcreate_ev_t*> &tmp_create_list, callout_ev_t *callout_event);
	bool connect_callcreate_for_callcancel(list<callcreate_ev_t *>&tmp_create_list, callcancel_ev_t *callcancel_event);
};
typedef TimercallPattern timercall_pattern_t;
#endif
