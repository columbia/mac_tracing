#ifndef MKRUN_WAIT_HPP
#define MKRUN_WAIT_HPP
#include "wait.hpp"
#include "mkrun.hpp"

class MkrunWaitPair {
	list<event_t *> &wait_list;
	list<event_t *> &mkrun_list;
public:
	MkrunWaitPair(list<event_t *> &_wait_list, list<event_t *> &_mkrun_list);
	void pair_wait_mkrun(void);
};

typedef MkrunWaitPair mkrun_wait_t;
#endif

