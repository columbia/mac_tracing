#include "dispatch_pattern.hpp"
#include "eventlistop.hpp"

#define DISP_PATTERN_DEBUG 0

DispatchPattern::DispatchPattern(std::list<EventBase*> &_enq_list, std::list<EventBase *> &_deq_list, std::list<EventBase*> &_exe_list)
:enqueue_list(_enq_list), dequeue_list(_deq_list), execute_list(_exe_list)
{
}

void DispatchPattern::connect_dispatch_patterns(void)
{
#ifdef DISP_PATTERN_DEBUG
    mtx.lock();
    LOG_S(INFO) << "begin matching dispatch... " << std::endl;
    mtx.unlock();
#endif
    connect_enq_and_deq();
    connect_deq_and_exe();
#ifdef DISP_PATTERN_DEBUG
    mtx.lock();
    LOG_S(INFO) << "finish matching dispatch. " << std::endl;
    mtx.unlock();
#endif
}

void DispatchPattern::connect_enq_and_deq(void)
{
    std::list<EventBase *> mix_list;
    std::list<BlockEnqueueEvent*> tmp_enq_list;
    BlockEnqueueEvent *enq_event;
    BlockDequeueEvent *deq_event;

    mix_list.insert(mix_list.end(), enqueue_list.begin(), enqueue_list.end());
    mix_list.insert(mix_list.end(), dequeue_list.begin(), dequeue_list.end());
    EventLists::sort_event_list(mix_list);
	
	for(auto event: mix_list) {
		switch(event->get_event_type()) {
			case DISP_ENQ_EVENT:
        		enq_event = dynamic_cast<BlockEnqueueEvent*>(event);
            	tmp_enq_list.push_back(enq_event);
				break;
			case DISP_DEQ_EVENT:
				deq_event = dynamic_cast<BlockDequeueEvent*>(event);
				connect_dispatch_enqueue_for_dequeue(tmp_enq_list, deq_event);
				break;
			default:
				break;
		}
	}

#if DISP_PATTERN_DEBUG
    //TODO: check the rest events
    std::list<EventBase *>::iterator it;
    for (it = enqueue_list.begin(); it != enqueue_list.end(); it++) {
        enq_event = dynamic_cast<BlockEnqueueEvent*>(*it);
        if (enq_event->is_consumed() == false) {
            mtx.lock();
            LOG_S(INFO) << "Warn: Not matched enqueue " << std::fixed << std::setprecision(1) << enq_event->get_abstime();
            LOG_S(INFO) << "\t" << std::hex << enq_event->get_tid() << "\t" << enq_event->get_ref()<< std::endl;
            mtx.unlock();
        }
    }
#endif
}

void DispatchPattern::connect_deq_and_exe(void)
{
    std::map<tid_t, std::list<EventBase *> > qevents_maps;
	for (auto event: dequeue_list) {
		tid_t tid = event->get_tid();
        if (qevents_maps.find(tid) != qevents_maps.end()) {
            std::list<EventBase *> tmp_list;
            qevents_maps[tid] = tmp_list;
        }
        qevents_maps[tid].push_back(event);
	}

	for (auto event: execute_list) {
		tid_t tid = event->get_tid();
        if (qevents_maps.find(tid) != qevents_maps.end()) {
            std::list<EventBase *> tmp_list;
            qevents_maps[tid] = tmp_list;
        }
        qevents_maps[tid].push_back(event);
	}
	
	for (auto element: qevents_maps) {
        std::list<EventBase *> cur_list = element.second;
        std::list<BlockDequeueEvent*> tmp_deq_list;
        EventLists::sort_event_list(cur_list);
		BlockDequeueEvent *deq_event;
		BlockInvokeEvent *invoke_event;
		for (auto event: cur_list) {
			switch (event->get_event_type()) {
				case DISP_DEQ_EVENT:
            		deq_event = dynamic_cast<BlockDequeueEvent *>(event);
                	tmp_deq_list.push_back(deq_event);
					break;
				case DISP_INV_EVENT:
                	invoke_event = dynamic_cast<BlockInvokeEvent*>(event);
                	if (invoke_event->is_begin()) 
                    	connect_dispatch_dequeue_for_execute(tmp_deq_list, invoke_event);
					break;
				default:
					break;
			}
        }
    }
}

bool DispatchPattern::connect_dispatch_enqueue_for_dequeue(std::list<BlockEnqueueEvent*> &tmp_enq_list, BlockDequeueEvent* dequeue_event)
{
    std::list<BlockEnqueueEvent *>::reverse_iterator enq_it;
    for (enq_it = tmp_enq_list.rbegin();
            enq_it != tmp_enq_list.rend(); enq_it++)  {
        BlockEnqueueEvent * enqueue_event = *enq_it;
        if (enqueue_event->get_qid() == dequeue_event->get_qid()
            && enqueue_event->get_item() == dequeue_event->get_item()) {
            dequeue_event->set_root(enqueue_event);
            enqueue_event->set_consumer(dequeue_event);
            tmp_enq_list.erase(next(enq_it).base());
            return true;;
        }
    }
#if DISP_PATTERN_DEBUG
    mtx.lock();
    LOG_S(INFO) << "Warn: no enqueue found for dequeue " << std::fixed << std::setprecision(1) << dequeue_event->get_abstime();
    LOG_S(INFO) << "\t" << std::hex << dequeue_event->get_tid() << std::endl;
    if (dequeue_event -> is_duplicate())
        LOG_S(INFO) << "Duplicate Dequeue at " << std::fixed << std::setprecision(1) << dequeue_event->get_abstime() << std::endl;
    mtx.unlock();
#endif
    return false;
}

bool DispatchPattern::connect_dispatch_dequeue_for_execute(std::list<BlockDequeueEvent*> &tmp_deq_list, BlockInvokeEvent * invoke_event)
{
    std::list<BlockDequeueEvent*>::reverse_iterator deq_it;
    BlockDequeueEvent * dequeue_event;
    for (deq_it = tmp_deq_list.rbegin(); 
            deq_it != tmp_deq_list.rend(); deq_it++) {
        assert((*deq_it)->get_tid() == invoke_event->get_tid());
        dequeue_event = *deq_it;
        if (dequeue_event->get_vtable_ptr() >= 0xff)
            continue;
        if (dequeue_event->get_func_ptr() == invoke_event->get_func() 
			|| dequeue_event->get_func_ptr() == invoke_event->get_ctxt()) {
            invoke_event->set_root(dequeue_event);
            dequeue_event->set_executed(invoke_event);
            tmp_deq_list.erase(next(deq_it).base());
            return true;
        } 
    }
#if DISP_PATTERN_DEBUG
    mtx.lock();
    LOG_S(INFO) << "Warn: no dequeue found for blockinvoke " << std::fixed << std::setprecision(1) << invoke_event->get_abstime();
    LOG_S(INFO) << "\t" << std::hex << invoke_event->get_tid() << std::endl;
    mtx.unlock();
#endif
    return false;
}
