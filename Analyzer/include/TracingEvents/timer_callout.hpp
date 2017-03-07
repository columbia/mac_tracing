#ifndef TIMER_CALLOUT_HPP
#define TIMER_CALLOUT_HPP

#include "base.hpp"
using namespace std;

class CallCreateEvent : public EventBase {
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
	bool is_called;
	bool is_cancelled;
	callcancel_ev_t * cancel_event;
	callout_ev_t * callout_event;
public:
	CallCreateEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
			void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void * get_q_ptr(void) {return q_ptr;}
	void set_called_peer(callout_ev_t * callout) {is_called = true; callout_event = callout;}
	bool check_called(void) {return is_called;}
	callout_ev_t * get_called_peer(void) {return callout_event;}
	void cancel_call(callcancel_ev_t * callcancel) {is_cancelled = true; cancel_event = callcancel;}
	callcancel_ev_t * get_cancel_peer(void) {return cancel_event;}
	bool check_cancel(void) {return is_cancelled;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class CalloutEvent : public EventBase {
	callcreate_ev_t * create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	CalloutEvent(double abstime, string op, uint64_t _tid,  uint32_t event_core,
			void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_callcreate(callcreate_ev_t * call_create) {create_event = call_create;}
	callcreate_ev_t * get_callcreate(void) {return create_event;}
	bool check_root(callcreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class CallCancelEvent : public EventBase {
	callcreate_ev_t * create_event;
	void *func_ptr;
	uint64_t param0;
	uint64_t param1;
	void *q_ptr;
public:
	CallCancelEvent(double abstime, string op, uint64_t _tid, uint32_t event_core,
						void * funcptr, uint64_t param0, uint64_t param1, void * qptr, string proc= "");
	void * get_func_ptr(void) {return func_ptr;}
	uint64_t get_param0(void) {return param0;}
	uint64_t get_param1(void) {return param1;}
	void set_callcreate(callcreate_ev_t * _create_event) {create_event = _create_event;}
	callcreate_ev_t * get_callcreate(void) {return create_event;}
	bool check_root(callcreate_ev_t *);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
