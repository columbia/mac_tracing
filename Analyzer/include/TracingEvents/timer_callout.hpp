#ifndef TIMER_CALLOUT_HPP
#define TIMER_CALLOUT_HPP

#include "base.hpp"
using namespace std;

class TimerCreateEvent : public EventBase {
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
	bool is_called;
	bool is_cancelled;
	timercancel_ev_t * cancel_event;
	timercallout_ev_t * timercallout_event;
public:
	TimerCreateEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
			void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void * get_q_ptr(void) {return q_ptr;}
	void set_called_peer(timercallout_ev_t * timercallout) {is_called = true; timercallout_event = timercallout;}
	bool check_called(void) {return is_called;}
	timercallout_ev_t * get_called_peer(void) {return timercallout_event;}
	void cancel_call(timercancel_ev_t * timercancel) {is_cancelled = true; cancel_event = timercancel;}
	timercancel_ev_t * get_cancel_peer(void) {return cancel_event;}
	bool check_cancel(void) {return is_cancelled;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class TimerCalloutEvent : public EventBase {
	timercreate_ev_t * create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	TimerCalloutEvent(double abstime, string op, uint64_t _tid,  uint32_t event_core,
			void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_timercreate(timercreate_ev_t * call_create) {create_event = call_create;}
	timercreate_ev_t * get_timercreate(void) {return create_event;}
	bool check_root(timercreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class TimerCancelEvent : public EventBase {
	timercreate_ev_t * create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	TimerCancelEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
						void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_timercreate(timercreate_ev_t * _create_event) {create_event = _create_event;}
	timercreate_ev_t * get_timercreate(void) {return create_event;}
	bool check_root(timercreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
