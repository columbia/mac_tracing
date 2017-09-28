#include "breakpoint_trap_connection.hpp"
#include "eventlistop.hpp"

#define DEBUG_BREAKPOINT_TRAP 1
BreakpointTrapConnection::BreakpointTrapConnection(list<event_t *> &_breakpoint_trap_list)
:breakpoint_trap_list(_breakpoint_trap_list)
{
}

#if 0
void BreakpointTrapConnection::breakpoint_trap_connection(void)
{
	EventListOp::sort_event_list(breakpoint_trap_list);

	list<event_t *>::iterator it;
	breakpoint_trap_ev_t *prev_write = NULL, *cur_event;

	for (it = breakpoint_trap_list.begin(); it != breakpoint_trap_list.end(); it++) {
		cur_event = dynamic_cast<breakpoint_trap_ev_t *>(*it);
		if (cur_event->get_procname() != "WindowServer")
			continue; //WindowServer first 

		if (cur_event->check_read() == true) {
			if (prev_write != NULL) {
				cur_event->set_peer(prev_write);
				prev_write = NULL;
			} else {
#if DEBUG_BREAKPOINT_TRAP 	
				cerr << "Pendout Event(Read) at " << fixed << setprecision(1) << cur_event->get_abstime() << " miss corresponding write" << endl;
#endif
			}
		} else {
			if (prev_write != NULL) {
#if DEBUG_BREAKPOINT_TRAP 	
				cerr << "Pendout Event(Write) at " << fixed << setprecision(1) << cur_event->get_abstime() << " miss corresponding read" << endl;
#endif
			}
			prev_write = cur_event;
		}
	}
}
#endif

void BreakpointTrapConnection::breakpoint_trap_connection(void)
{
	map<string, uint32_t> prev_values;
	map<string, breakpoint_trap_ev_t *> prev_writes;

	list<event_t *>::iterator it;
	breakpoint_trap_ev_t *cur_event;

	EventListOp::sort_event_list(breakpoint_trap_list);
	prev_values.clear();
	prev_writes.clear();
	
	for (it = breakpoint_trap_list.begin(); it != breakpoint_trap_list.end(); it++) {
		cur_event = dynamic_cast<breakpoint_trap_ev_t *>(*it);
		map<string, uint32_t> cur_targets = cur_event->get_targets();
		map<string, uint32_t>::iterator target_it;
		for (target_it = cur_targets.begin(); target_it != cur_targets.end(); target_it++) {
			string var = target_it->first;
			uint32_t val = target_it->second;
			if (prev_values.find(var) != prev_values.end() && prev_values[var] != val) {
				if (val == 1) {
					cur_event->set_read(false);
					if (prev_writes.find(var) != prev_writes.end()) {
#if DEBUG_BREAKPOINT_TRAP 	
				cerr << "BreakpointTrap Event(Write) at " << fixed << setprecision(1) << cur_event->get_abstime() << " miss corresponding read" << endl;
#endif
					}
					prev_writes[var] = cur_event;
				} else { // this is a read
					cur_event->set_read(true);
					if (prev_writes.find(var) != prev_writes.end()) {
						cur_event->set_peer(prev_writes[var]);
						prev_writes[var]->set_peer(cur_event);
						prev_writes.erase(var);
					} else {
#if DEBUG_BREAKPOINT_TRAP 	
				cerr << "BreakpointTrap Event(Read) at " << fixed << setprecision(1) << cur_event->get_abstime() << " miss corresponding write" << endl;
#endif
					}
				}
			}
			prev_values[var] = val;
		}
	}
	prev_values.clear();
	prev_writes.clear();
}
