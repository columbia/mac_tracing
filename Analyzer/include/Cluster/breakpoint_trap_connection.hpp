#ifndef BREAKPOINT_TRAP_CONNECTION_HPP
#define	BREAKPOINT_TRAP_CONNECTION_HPP
#include "breakpoint_trap.hpp"

class BreakpointTrapConnection {
	list<event_t *> &breakpoint_trap_list;
public:
	BreakpointTrapConnection(list<event_t *> &_breakpoint_trap_list);
	void breakpoint_trap_connection(void);
};
typedef BreakpointTrapConnection breakpoint_trap_connection_t;
#endif
