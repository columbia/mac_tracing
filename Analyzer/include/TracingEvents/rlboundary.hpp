#ifndef RLBOUNDARY_HPP
#define RLBOUNDARY_HPP
#include "base.hpp"
#define CategoryBegin	0
#define CategoryEnd		1
#define ItemBegin		2
#define ItemEnd			3
#define SetSignalForSource0		4
#define UnsetSignalForSource0	5
#define BlockPerform	7

class RLBoundaryEvent:public EventBase {
	uint32_t state;
	uint64_t func_ptr;
	string func_symbol;
	event_t *owner;
	uint64_t rls;
	uint64_t block;
public:
	RLBoundaryEvent(double timestamp, string op, uint64_t tid, uint32_t state, uint64_t func_ptr, uint32_t event_core, string procname="");
	void set_func_symbol(string _symbol) {func_symbol = _symbol;}
	void set_rls(uint64_t _rls) {rls = _rls;}
	void set_block(uint64_t _block) {block = _block;}
	uint64_t get_state(void) {return state;}
	uint64_t get_func_ptr(void) {return func_ptr;}
	uint64_t get_rls(void) {return rls;}
	uint64_t get_block(void) {return block;}
	void set_owner(event_t *_owner) {owner = _owner;}
	event_t *get_owner(void) {return owner;}
	const char * decode_state(int state);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
