#ifndef DISPATCH_PATTERN_HPP
#define DISPATCH_PATTERN_HPP
#include "dispatch.hpp"
class DispatchPattern {
    std::list<EventBase *> &enqueue_list;
    std::list<EventBase *> &dequeue_list;
    std::list<EventBase *> &execute_list;
public:
    //DispatchPattern(std::list<BlockEnqueueEvent*> &_enq_list, std::list<BlockDequeueEvent *> &_deq_list, std::list<BlockInvokeEvent*> &_exe_list);
    DispatchPattern(std::list<EventBase*> &_enq_list, std::list<EventBase *> &_deq_list, std::list<EventBase*> &_exe_list);
    void connect_dispatch_patterns();
    void connect_enq_and_deq();
    void connect_deq_and_exe();
    bool connect_dispatch_enqueue_for_dequeue(std::list<BlockEnqueueEvent*>&tmp_enq_list, BlockDequeueEvent*);
    bool connect_dispatch_dequeue_for_execute(std::list<BlockDequeueEvent *>&tmp_deq_list, BlockInvokeEvent*);
};
typedef DispatchPattern dispatch_pattern_t;
#endif
