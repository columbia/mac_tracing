#ifndef RL_CONNECTION_HPP
#define RL_CONNECTION_HPP
#include "rlboundary.hpp"
class RLConnection {
	typedef map<uint64_t, list<event_t *> > tid_evlist_t;
	tid_evlist_t tid_lists;
	list<event_t *> rl_item_list;
public:
	RLConnection(list<event_t *> &rl_item_list, tid_evlist_t &tid_lists);
	void connect_for_source1(rl_boundary_ev_t *rl_boundary_event);
	void connect_for_source0(rl_boundary_ev_t *rl_boundary_event, list<event_t *>::reverse_iterator rit);
	void connect_for_blocks(rl_boundary_ev_t *rl_boundary_event, list<event_t *>::reverse_iterator rit);
	void rl_connection(void);
};
typedef RLConnection rl_connection_t;
#endif
