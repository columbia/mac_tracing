#include "timercall_pattern.hpp"
#include "eventlistop.hpp"

#define TIMERCALL_DEBUG 0

TimerCallPattern::TimerCallPattern(std::list<EventBase*> &_timercreate_list, std::list<EventBase *> &_timercallout_list, std::list<EventBase*> &_timercancel_list)
:timercreate_list(_timercreate_list), timercallout_list(_timercallout_list), timercancel_list(_timercancel_list)
{
}

void TimerCallPattern::connect_timercall_patterns(void)
{
#ifdef TIMERCALL_DEBUG
    mtx.lock();
    std::cerr << "begin matching timercall pattern ... " << std::endl;
    mtx.unlock();
#endif
    connect_create_and_cancel();
    connect_create_and_timercallout();
#ifdef TIMERCALL_DEBUG
    mtx.lock();
    std::cerr << "finish matching timercall pattern. " << std::endl;
    mtx.unlock();
#endif
}

void TimerCallPattern::connect_create_and_cancel(void)
{
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> mix_list;
    std::list<TimerCreateEvent*> tmp_create_list;
    TimerCreateEvent * timercreate_event;
    TimerCancelEvent * timercancel_event;

    mix_list.insert(mix_list.end(), timercreate_list.begin(), timercreate_list.end());
    mix_list.insert(mix_list.end(), timercancel_list.begin(), timercancel_list.end());
    EventLists::sort_event_list(mix_list);

    for (it = mix_list.begin(); it != mix_list.end(); it++) {
        timercreate_event = dynamic_cast<TimerCreateEvent *>(*it);
        if (timercreate_event) {
            tmp_create_list.push_back(timercreate_event);
        } else {
            timercancel_event = dynamic_cast<TimerCancelEvent *>(*it);
            connect_timercreate_for_timercancel(tmp_create_list, timercancel_event);
        }
    }
    //TODO : check the rest events
}

bool TimerCallPattern::connect_timercreate_for_timercancel(std::list<TimerCreateEvent *>&tmp_create_list, TimerCancelEvent *timercancel_event)
{
    std::list<TimerCreateEvent *>::reverse_iterator rit;
    for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
        TimerCreateEvent * timercreate_event = dynamic_cast<TimerCreateEvent *>(*rit);
        if (timercancel_event->check_root(timercreate_event) == true) {
            timercancel_event->set_timercreate(timercreate_event);
            timercreate_event->cancel_call(timercancel_event);
            tmp_create_list.erase(next(rit).base());
            return true;
        }
    }
#if TIMERCALL_DEBUG
    mtx.lock();
    std::cerr << "Warn: no timercreate for timercancel " << std::fixed << std::setprecision(1) << timercancel_event->get_abstime() << std::endl; 
    mtx.unlock();
#endif
    return false;
}

void TimerCallPattern::connect_create_and_timercallout(void)
{
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> mix_list;
    std::list<TimerCreateEvent*> tmp_create_list;
    TimerCreateEvent * timercreate_event;
    TimerCalloutEvent * timercallout_event;

    mix_list.insert(mix_list.end(), timercreate_list.begin(), timercreate_list.end());
    mix_list.insert(mix_list.end(), timercallout_list.begin(), timercallout_list.end());
    EventLists::sort_event_list(mix_list);
    for (it = mix_list.begin(); it != mix_list.end(); it++) {
        timercreate_event = dynamic_cast<TimerCreateEvent *>(*it);
        if (timercreate_event) {
            tmp_create_list.push_back(timercreate_event);
        } else {
            timercallout_event = dynamic_cast<TimerCalloutEvent*>(*it);
            connect_timercreate_for_timercallout(tmp_create_list, timercallout_event);
        }
    }
}

bool TimerCallPattern::connect_timercreate_for_timercallout(std::list<TimerCreateEvent*> &tmp_create_list, TimerCalloutEvent *timercallout_event)
{
    std::list<TimerCreateEvent *>::reverse_iterator rit;
    for (rit = tmp_create_list.rbegin(); rit != tmp_create_list.rend(); rit++) {
        TimerCreateEvent *timercreate_event = dynamic_cast<TimerCreateEvent *>(*rit);
        if (timercallout_event->check_root(timercreate_event) == true) {
            timercallout_event->set_timercreate(timercreate_event);
            timercreate_event->set_called_peer(timercallout_event);
            tmp_create_list.erase(next(rit).base());
            return true;
        }
    }

#if TIMERCALL_DEBUG
    mtx.lock();
    std::cerr << "Warn: no timercreate for timercallout " << std::fixed << std::setprecision(1) << timercallout_event->get_abstime() << std::endl; 
    mtx.unlock();
#endif
    return false;
}
