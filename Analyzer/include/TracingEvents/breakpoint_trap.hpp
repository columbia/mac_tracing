#ifndef BREAKPOINT_TRAP_HPP
#define BREAKPOINT_TRAP_HPP
#include "base.hpp"
using namespace std;

//#define ReadPendingMsg_0	"CGXRunOneServicesPass"
//#define ReadPendingMsg_1	"push_out_message"
//#define WritePendingMsg		"CGXPostReplyMessage"

class BreakpointTrapEvent : public EventBase {
	bool is_read;
	uint64_t caller_eip;
	string caller;
	msg_ev_t *pending_message;
	backtrace_ev_t *backtrace;
	breakpoint_trap_ev_t *rw_peer;
	vector<uint64_t> addrs;
	vector<uint32_t> vals;
	map<string, uint32_t> targets;

public:
	BreakpointTrapEvent(double timestamp, string op, uint64_t tid, uint64_t eip, uint32_t coreid, string procname = "");
	void set_caller_info(string _caller) {caller = _caller;}
	void add_addr(uint64_t addr) {addrs.push_back(addr);}
	void add_value(uint32_t val) {vals.push_back(val);}
	void update_target(int index, string key);
	uint64_t get_eip(void) {return caller_eip;}
	vector<uint64_t> &get_addrs(void) {return addrs;}

	void set_backtrace(backtrace_ev_t *bt) {backtrace = bt;}
	void set_pending_msg(msg_ev_t *msg) {pending_message = msg;}

	// referred by breakpoint connection
	map<string, uint32_t> &get_targets(void) {return targets;}
	void set_read(bool read) {is_read = read;}
	bool check_read(void) {return is_read;}
	void set_peer(breakpoint_trap_ev_t *breakpoint_trap) {rw_peer = breakpoint_trap;}
	breakpoint_trap_ev_t *get_peer(void) {return rw_peer;}

	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
