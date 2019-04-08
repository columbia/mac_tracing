#ifndef BREAKPOINT_TRAP_CONNECTION_HPP
#define	BREAKPOINT_TRAP_CONNECTION_HPP
#include "breakpoint_trap.hpp"

#define FLIP_VAR 0
#define QUEUE_VAR 1

typedef map<string, vector<breakpoint_trap_ev_t *> > queue_hw_write_t;

class BreakpointTrapConnection {
	list<event_t *> &breakpoint_trap_list;
	map<string, uint32_t> var_type;
public:
	BreakpointTrapConnection(list<event_t *> &_breakpoint_trap_list);
//alternatively changing flag. outpending_msg, setspinning
	void flip_var_connection(string var, uint32_t prev_val, uint32_t val,
		queue_hw_write_t &prev_writes, breakpoint_trap_ev_t *cur_event);
//type 1 queued flag, dispatchToMainThread
	void queue_var_connection(string var, uint32_t prev_val, uint32_t val,
		queue_hw_write_t &prev_writes, breakpoint_trap_ev_t *cur_event);
	void breakpoint_trap_connection(void);
};
typedef BreakpointTrapConnection breakpoint_trap_connection_t;
#endif
