#include "rlboundary.hpp"

RLBoundaryEvent::RLBoundaryEvent(double timestamp, string op, uint64_t tid, uint32_t _state, uint64_t _func_ptr, uint32_t event_core, string procname)
:EventBase(timestamp, RL_BOUNDARY_EVENT, op, tid, event_core, procname)
{
	state = _state;
	func_ptr = _func_ptr;
	consumer = owner = NULL;
	rls = 0;
	block = 0;
}

const char * RLBoundaryEvent::decode_state(int state)
{
	switch(state) {
		case CategoryBegin:
			return "CategoryBegin";
		case CategoryEnd:
			return "CategoryEnd";
		case ItemBegin:
			return "ItemBegin";
		case ItemEnd:
			return "ItemEnd";
		case SetSignalForSource0:
			return "SetRLSource0";
		case UnsetSignalForSource0:
			return "UnsetRLSource0";
		case BlockPerform:
			return "BlockPerform";
		default:
			return "unknown";
	}
}

void RLBoundaryEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\nstate = " << decode_state(state) << endl;
	if (func_symbol.size())
		outfile << "\nCallback = " << func_symbol << endl;
	if (owner)
		outfile << "\nConnected by event at " << fixed << setprecision(1) << owner->get_abstime() << endl;
	if (rls)
		outfile << "\nRls = " << rls << endl; 
}

void RLBoundaryEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tstate = " << decode_state(state);
	if (func_symbol.size())
		outfile << "\n\tCallback = " << func_symbol;
	if (owner)
		outfile << "\n\tConnected by event at " << fixed << setprecision(1) << owner->get_abstime();
	if (rls)
		outfile << "\n\tRls = " << rls;
	if (block)
		outfile << "\n\tblock = " << block;
	outfile << endl;
}
