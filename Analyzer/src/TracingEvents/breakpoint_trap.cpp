#include "breakpoint_trap.hpp"
BreakpointTrapEvent::BreakpointTrapEvent(double abstime, string _op, uint64_t _tid, uint64_t eip,  uint32_t _core_id, string _procname)
:EventBase(abstime, BREAKPOINT_TRAP_EVENT, _op, _tid, _core_id, _procname)
{
	caller_eip = eip;
	is_read = false;
	caller = "";
	rw_peer = NULL;
	addrs.clear();
	vals.clear();
	targets.clear();
}

void BreakpointTrapEvent::update_target(int index, string key)
{
	if (index < vals.size())
		targets[key] = vals[index];
	else
		cerr << __func__ << ": Key - value are not matched" << endl;
}

void BreakpointTrapEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tcaller " << caller << endl;
	
	map<string, uint32_t>::iterator it;
	for (it = targets.begin(); it != targets.end(); it++) {
		outfile << "\n\t" << it->first << " = " << it->second << endl;
	}
}

void BreakpointTrapEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << caller;
	
	vector<uint64_t>::iterator addr_it;
	for (addr_it = addrs.begin(); addr_it != addrs.end(); addr_it++) {
		outfile << "\t" << *addr_it;
	}
	
	map<string, uint32_t>::iterator it;
	for (it = targets.begin(); it != targets.end(); it++) {
		outfile << "\t" << it->first << " = " << it->second;
	}
	outfile << endl;
}
