#ifndef MKRUN_WAIT_HPP
#define MKRUN_WAIT_HPP
#include "wait.hpp"
#include "mkrunnable.hpp"

class MakeRunnableWaitPair {
    std::list<EventBase *> &wait_list;
    std::list<EventBase *> &mkrun_list;
public:
    MakeRunnableWaitPair(std::list<EventBase *> &_wait_list, std::list<EventBase *> &_mkrun_list);
    void pair_wait_mkrun(void);
};

typedef MakeRunnableWaitPair mkrun_wait_t;
#endif

