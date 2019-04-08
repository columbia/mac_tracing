#ifndef TIMER_CALLOUT_HPP
#define TIMER_CALLOUT_HPP

#include "base.hpp"
using namespace std;


class TimerCalloutEvent : public EventBase {
	timercreate_ev_t *create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	TimerCalloutEvent(double abstime, string op, uint64_t _tid,  uint32_t event_core,
			void *funcptr, uint64_t param0, uint64_t param1, void *qptr, string proc= "");
	void *get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_timercreate(timercreate_ev_t * call_create) {create_event = call_create;}
	void unset_timercreate(void) {create_event = NULL;}
	timercreate_ev_t *get_timercreate(void) {return create_event;}
	bool check_root(timercreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class TimerCancelEvent : public EventBase {
	timercreate_ev_t *create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	TimerCancelEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
						void *funcptr, uint64_t param0, uint64_t param1, void *qptr, string proc= "");
	void *get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_timercreate(timercreate_ev_t *_create_event) {create_event = _create_event;}
	void unset_timercreate(void) {create_event = NULL;}
	timercreate_ev_t *get_timercreate(void) {return create_event;}
	bool check_root(timercreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class TimerCreateEvent : public EventBase {
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
	bool is_called;
	bool is_cancelled;
	bool is_event_processing;
	timercancel_ev_t *cancel_event;
	timercallout_ev_t *timercallout_event;
public:
	TimerCreateEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
			void *funcptr, uint64_t param0, uint64_t param1, void *qptr, string proc= "");
	void *get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void *get_q_ptr(void) {return q_ptr;}
	void set_called_peer(timercallout_ev_t * timercallout) {is_called = true; timercallout_event = timercallout;}
	void unset_called_peer(void) {
		is_called = false;
		if (timercallout_event != NULL)
			timercallout_event->unset_timercreate();
		timercallout_event = NULL;
	}
	bool check_called(void) {return is_called;}
	timercallout_ev_t *get_called_peer(void) {return timercallout_event;}
	void cancel_call(timercancel_ev_t * timercancel) {is_cancelled = true; cancel_event = timercancel;}
	void unset_cancel_call(void) {
		is_cancelled = false;
		if (cancel_event != NULL)
			cancel_event->unset_timercreate();
		cancel_event = NULL;
	}
	timercancel_ev_t *get_cancel_peer(void) {return cancel_event;}
	bool check_cancel(void) {return is_cancelled;}
	void set_is_event_processing(void) {is_event_processing = true;}
	bool check_event_processing() {return is_event_processing;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
