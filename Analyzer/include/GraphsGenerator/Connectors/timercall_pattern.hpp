#ifndef TIMER_CALL_PATTERN_HPP
#define TIMER_CALL_PATTERN_HPP
#include "timer_call.hpp"
class TimerCallPattern {
    std::list<EventBase *> &timercreate_list;
    std::list<EventBase *> &timercallout_list;
    std::list<EventBase *> &timercancel_list;
public:
    TimerCallPattern(std::list<EventBase*> &_timercreate_list, std::list<EventBase *> &_timercallout_list, std::list<EventBase*> &_timercancel_list);
    void connect_timercall_patterns(void);
    void connect_create_and_cancel(void);
    void connect_create_and_timercallout(void);
    bool connect_timercreate_for_timercallout(std::list<TimerCreateEvent*> &tmp_create_list, TimerCalloutEvent *timercallout_event);
    bool connect_timercreate_for_timercancel(std::list<TimerCreateEvent *>&tmp_create_list, TimerCancelEvent *timercancel_event);
};
typedef TimerCallPattern timercall_pattern_t;
#endif
