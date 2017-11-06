#include "breakpoint_trap_connection.hpp"
#include "eventlistop.hpp"

#define DEBUG_BREAKPOINT_TRAP 0

BreakpointTrapConnection::BreakpointTrapConnection(list<event_t *> &_breakpoint_trap_list)
:breakpoint_trap_list(_breakpoint_trap_list)
{
	var_type.clear();
	var_type["gOutMsgPending"] = FLIP_VAR;
	var_type["_ZL28sCGEventIsMainThreadSpinning"] = FLIP_VAR;
	var_type["_ZL32sCGEventIsDispatchedToMainThread"] = QUEUE_VAR;
}

void BreakpointTrapConnection::flip_var_connection(string var, uint32_t prev_val, uint32_t val,
		queue_hw_write_t &prev_writes, breakpoint_trap_ev_t *cur_event)
{
	if (prev_val == 0 && val == 1) { //store writes
		cur_event->set_read(false);
		if (prev_writes.find(var) == prev_writes.end()) {
			vector<breakpoint_trap_ev_t *> container;
			prev_writes[var] = container;
		}
		prev_writes[var].push_back(cur_event);
		cur_event->set_trigger_var(var);
#if DEBUG_BREAKPOINT_TRAP
		mtx.lock();
		if (var == "gOutMsgPending") {
			cerr << "Set write ?";
			if (cur_event->check_read())
				cerr << "N";
			else
				cerr << "Y";
			cerr << "\t val = " << cur_event->get_trigger_val() << endl;
		}
		mtx.unlock();
#endif
	} else if (prev_val == 1 && val == 0) {
		cur_event->set_read(true);
		if (prev_writes.find(var) != prev_writes.end() && (prev_writes[var].size())) {
			assert(prev_writes[var].size() == 1);
			cur_event->set_peer(prev_writes[var][0]);
			prev_writes[var][0]->set_peer(cur_event);
			prev_writes[var].erase(prev_writes[var].begin());
		}
		cur_event->set_trigger_var(var);
#if DEBUG_BREAKPOINT_TRAP
		mtx.lock();
		if (var == "gOutMsgPending") {
			cerr << "Set read ?";
			if (cur_event->check_read())
				cerr << "Y";
			else
				cerr << "N";
			cerr << "\t val = " << cur_event->get_trigger_val() << endl;
		}
		mtx.unlock();
#endif
	} else {
#if DEBUG_BREAKPOINT_TRAP
		mtx.lock();
		cerr << "No Flip expected at current event at " << fixed << setprecision(1) << cur_event->get_abstime() << endl; 
		mtx.unlock();
#endif
	}
}

void BreakpointTrapConnection::queue_var_connection(string var, uint32_t prev_val, uint32_t val,
		queue_hw_write_t &prev_writes, breakpoint_trap_ev_t *cur_event)
{
	if (val == 1) { //store writes
		cur_event->set_read(false);
		if (prev_writes.find(var) == prev_writes.end()) {
			vector<breakpoint_trap_ev_t *> container;
			prev_writes[var] = container;
		}
		prev_writes[var].push_back(cur_event);
	} else {
		cur_event->set_read(true);
		if (prev_writes.find(var) != prev_writes.end() && (prev_writes[var].size())) {
			cur_event->set_peer(prev_writes[var][0]);
			cur_event->set_trigger_var(var);
			prev_writes[var][0]->set_peer(cur_event);
			prev_writes[var][0]->set_trigger_var(var);
			prev_writes[var].erase(prev_writes[var].begin());
		}
	}
}

void BreakpointTrapConnection::breakpoint_trap_connection(void)
{
	map<string, uint32_t> prev_values;
	map<string, vector<breakpoint_trap_ev_t *> > prev_writes;
	map<string, vector<breakpoint_trap_ev_t *> >::iterator vec_it;

	list<event_t *>::iterator it;
	breakpoint_trap_ev_t *cur_event;

	EventListOp::sort_event_list(breakpoint_trap_list);
	prev_values.clear();
	prev_writes.clear();
	
#ifdef DEBUG_BREAKPOINT_TRAP
	mtx.lock();
	cerr << "begin breakpoint trap (shared variable) connection... " << endl;
	mtx.unlock();
#endif
	for (it = breakpoint_trap_list.begin(); it != breakpoint_trap_list.end(); it++) {
		cur_event = dynamic_cast<breakpoint_trap_ev_t *>(*it);
		map<string, uint32_t> cur_targets = cur_event->get_targets();
		map<string, uint32_t>::iterator target_it;
		for (target_it = cur_targets.begin(); target_it != cur_targets.end(); target_it++) {
			string var = target_it->first;
			uint32_t val = target_it->second;
			uint32_t prev_val = prev_values.find(var) != prev_values.end() ? prev_values[var] : 0;
			
			switch (var_type[var]) {
				case FLIP_VAR:
					flip_var_connection(var, prev_val, val, prev_writes, cur_event);
					break;
				case QUEUE_VAR:
					queue_var_connection(var, prev_val, val, prev_writes, cur_event);
					break;
				default:
					break;
			}
			prev_values[var] = val;
		}
	}

	for (vec_it = prev_writes.begin(); vec_it != prev_writes.end(); vec_it++)
		(vec_it->second).clear();
	
	prev_writes.clear();
#ifdef DEBUG_BREAKPOINT_TRAP
	mtx.lock();
	cerr << "finish breakpoint trap connection." << endl;
	mtx.unlock();
#endif
}
