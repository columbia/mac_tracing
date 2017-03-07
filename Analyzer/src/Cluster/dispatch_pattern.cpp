#include "dispatch_pattern.hpp"
#include "eventlistop.hpp"

//#define DISP_PATTERN_DEBUG

DispatchPattern::DispatchPattern(list<event_t*> &_enq_list, list<event_t *> &_deq_list, list<event_t*> &_exe_list)
:enqueue_list(_enq_list), dequeue_list(_deq_list), execute_list(_exe_list)
{
}

void DispatchPattern::connect_enq_and_deq(void)
{
	list<event_t *> mix_list;
	list<event_t *>::iterator it;
	list<enqueue_ev_t*> tmp_enq_list;
	enqueue_ev_t * enq_event;
	dequeue_ev_t * deq_event;

	mix_list.insert(mix_list.end(), enqueue_list.begin(), enqueue_list.end());
	mix_list.insert(mix_list.end(), dequeue_list.begin(), dequeue_list.end());
	EventListOp::sort_event_list(mix_list);

	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		enq_event = dynamic_cast<enqueue_ev_t*>(*it);
		if (enq_event) {
			tmp_enq_list.push_back(enq_event);
		} else {
			deq_event = dynamic_cast<dequeue_ev_t*>(*it);
			connect_dispatch_enqueue_for_dequeue(tmp_enq_list, deq_event);
		}
	}
	#ifdef DISP_PATTERN_DEBUG

	//TODO: check the rest events
	for (it = enqueue_list.begin(); it != enqueue_list.end(); it++) {
		enq_event = dynamic_cast<enqueue_ev_t*>(*it);
		if (enq_event->is_consumed() == false) {
			cerr << "Warn: Not matched enqueue " << fixed << setprecision(1) << enq_event->get_abstime();
			cerr << "\t" << hex << enq_event->get_tid() << "\t" << enq_event->get_ref()<< endl;
		}
	}
	#endif
}

void DispatchPattern::connect_deq_and_exe(void)
{
	list<event_t *> mix_list;
	list<event_t *>::iterator it;
	list<dequeue_ev_t *> tmp_deq_list;
	dequeue_ev_t * deq_event;
	blockinvoke_ev_t * invoke_event;

	mix_list.insert(mix_list.end(), dequeue_list.begin(), dequeue_list.end());
	mix_list.insert(mix_list.end(), execute_list.begin(), execute_list.end());
	EventListOp::sort_event_list(mix_list);
	
	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		deq_event = dynamic_cast<dequeue_ev_t *>(*it);
		if (deq_event) {
			tmp_deq_list.push_back(deq_event);
		} else {
			invoke_event = dynamic_cast<blockinvoke_ev_t*>(*it);
			if (invoke_event->is_begin()) 
				connect_dispatch_dequeue_for_execute(tmp_deq_list, invoke_event);
		}
	}
	//TODO: check the rest events
}

bool DispatchPattern::connect_dispatch_enqueue_for_dequeue(list<enqueue_ev_t*> &tmp_enq_list, dequeue_ev_t* dequeue_event)
{
	list<enqueue_ev_t *>::reverse_iterator enq_it;
	for (enq_it = tmp_enq_list.rbegin();
			enq_it != tmp_enq_list.rend(); enq_it++)  {
		enqueue_ev_t * enqueue_event = *enq_it;
		if (enqueue_event->get_qid() == dequeue_event->get_qid()
			&& enqueue_event->get_item() == dequeue_event->get_item()) {
			dequeue_event->set_root(enqueue_event);
			enqueue_event->set_consumer(dequeue_event);
			tmp_enq_list.erase(next(enq_it).base());
			return true;;
		}
	}
	#ifdef DISP_PATTERN_DEBUG
	cerr << "Warn: no enqueue found for dequeue " << fixed << setprecision(1) << dequeue_event->get_abstime();
	cerr << "\t" << hex << dequeue_event->get_tid() << endl;
	if (dequeue_event -> is_duplicate())
		cerr << "Duplicate Dequeue at " << fixed << setprecision(1) << dequeue_event->get_abstime() << endl;
	#endif
	return false;
}

bool DispatchPattern::connect_dispatch_dequeue_for_execute(list<dequeue_ev_t*> &tmp_deq_list, blockinvoke_ev_t * invoke_event)
{
	list<dequeue_ev_t*>::reverse_iterator deq_it;
	dequeue_ev_t * dequeue_event;
	for (deq_it = tmp_deq_list.rbegin(); 
			deq_it != tmp_deq_list.rend(); deq_it++) {
		if ((*deq_it)->get_tid() != invoke_event->get_tid())
			continue;
		dequeue_event = *deq_it;
		if (dequeue_event->get_vtable_ptr() >= 0xff)
			continue;
		if (dequeue_event->get_func_ptr() == invoke_event->get_func()) {
			invoke_event->set_root(dequeue_event);
			dequeue_event->set_executed(invoke_event);
			tmp_deq_list.erase(next(deq_it).base());
			return true;
		} 
	}
	#ifdef DISP_PATTERN_DEBUG
	cerr << "Warn: no dequeue found for blockinvoke " << fixed << setprecision(1) << invoke_event->get_abstime();
	cerr << "\t" << hex << invoke_event->get_tid() << endl;
	#endif
	return false;
}
