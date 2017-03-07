#ifndef DISPATCH_PATTERN_HPP
#define DISPATCH_PATTERN_HPP
#include "dispatch.hpp"
class DispatchPattern {
	list<event_t *> &enqueue_list;
	list<event_t *> &dequeue_list;
	list<event_t *> &execute_list;
public:
	//DispatchPattern(list<enqueue_ev_t*> &_enq_list, list<dequeue_ev_t *> &_deq_list, list<blockinvoke_ev_t*> &_exe_list);
	DispatchPattern(list<event_t*> &_enq_list, list<event_t *> &_deq_list, list<event_t*> &_exe_list);
	void connect_dispatch_patterns() {connect_enq_and_deq(); connect_deq_and_exe();}
	void connect_enq_and_deq();
	void connect_deq_and_exe();
	bool connect_dispatch_enqueue_for_dequeue(list<enqueue_ev_t*>&tmp_enq_list, dequeue_ev_t*);
	bool connect_dispatch_dequeue_for_execute(list<dequeue_ev_t *>&tmp_deq_list, blockinvoke_ev_t*);
};
typedef DispatchPattern dispatch_pattern_t;
#endif
