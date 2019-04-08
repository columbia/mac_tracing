#ifndef DISPATCH_HPP
#define DISPATCH_HPP
#include "base.hpp"

using namespace std;

class EnqueueEvent: public EventBase {
	uint64_t q_id;
	uint64_t item;
	uint32_t ref;
	uint64_t nested_level;
	bool consumed;
	dequeue_ev_t *consumer;
public:
	EnqueueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint32_t _ref, uint32_t _core, string procname= "");
	bool is_consumed(void) {return consumed;}
	void set_consumer(dequeue_ev_t *_consumer) {consumed = true; consumer = _consumer;}
	dequeue_ev_t *get_consumer(void) {return consumer;}
	uint64_t get_qid(void) {return q_id;}
	uint64_t get_item(void) {return item;}
	uint32_t get_ref(void) {return ref;}
	void set_nested_level(uint64_t level) {nested_level = level;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

#define ROOTQ_DRAIN 	3
#define ROOTQ_INVOKE	4
#define Q_DRAIN			5
#define MAINQ_CB		21
#define RLQ_PF			23

class DequeueEvent: public EventBase {
	uint64_t q_id;
	uint64_t item;
	uint64_t ctxt;
	uint64_t func_ptr;
	uint64_t invoke_ptr;
	uint64_t vtable_ptr;
	uint32_t ref;
	uint64_t nested_level;
	bool duplicate;
	enqueue_ev_t *root;
	bool executed;
	blockinvoke_ev_t *invoke;
	string desc;
public:
	DequeueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint64_t _ctxt, uint32_t _ref, uint32_t _core, string proc="" );
	void set_ptrs(uint64_t _func_ptr, uint64_t _invoke_ptr, uint64_t _vtable_ptr);
	void set_root(enqueue_ev_t* _root) {root = _root;}
	enqueue_ev_t *get_root(void) {return root;}
	blockinvoke_ev_t *get_invoke(void) {return invoke;}
	uint64_t get_func_ptr(void) {return func_ptr;}
	uint64_t get_invoke_ptr(void) {return invoke_ptr;}
	uint64_t get_vtable_ptr(void) {return vtable_ptr;}
	void set_duplicate(void) {duplicate = true;}
	void set_executed(blockinvoke_ev_t *_invoke) {executed = true; invoke = _invoke;}
	void set_nested_level(uint64_t level) {nested_level = level;}
	bool is_duplicate(void) {return duplicate;}
	bool is_executed(void) {return executed;}
	uint64_t get_qid(void) {return q_id;}
	uint64_t get_item(void) {return item;}
	uint64_t get_ctxt(void) {return ctxt;}
	uint32_t get_ref(void) {return ref;}
	void set_desc(string &_desc) {desc = _desc;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class BlockinvokeEvent: public EventBase {
	uint64_t func;
	uint64_t ctxt;
	string desc;
	bool begin;
	bool rooted;
	event_t *root;
	backtrace_ev_t *bt;
	uint64_t nested_level;
public:
	BlockinvokeEvent(double abstime, string op, uint64_t _tid, uint64_t _func, uint64_t _ctxt, bool _begin, uint32_t _core_id, string proc="");
	uint64_t get_ctxt(void) {return ctxt;}
	uint64_t get_func(void) {return func;}
	void set_desc(string &_desc) {desc = _desc;}
	string get_desc(void) {return desc;}
	bool is_rooted(void) {return rooted;}
	bool is_begin(void) {return begin;}
	void set_root(event_t *_root) {rooted = true; root = _root;}
	void set_bt (backtrace_ev_t *_bt) {bt = _bt;}
	backtrace_ev_t *get_bt(void) {return bt;}
	event_t *get_root(void) {return root;}
	void set_nested_level(uint64_t level) {nested_level = level;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class DispMigEvent: public EventBase {
	blockinvoke_ev_t *invoker;
	void *owner;
public:
	DispMigEvent(double abstime, string op, uint64_t tid, uint32_t core_id, string procname);
	void save_owner(void *_owner);
	void set_mig_invoker(blockinvoke_ev_t *_invoker) {invoker = _invoker;}
	blockinvoke_ev_t *get_mig_invoker() {return invoker;}
	void *restore_owner(void);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
