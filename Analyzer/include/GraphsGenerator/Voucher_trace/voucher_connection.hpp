#ifndef VOUCHER_CONNECTION
#define VOUCHER_CONNECTION
#include "voucher.hpp"
#include "mach_msg.hpp"
#include <vector>

#define HOLD_SEND 	1
#define HOLD_RECV	2
#define IN_CREATE	3
#define IN_REUSE	4
#define IN_COPY		5
#define DEALLOC		6

class IPCNode;

struct voucher_state{
	uint64_t holder	: 56,
			 status : 8;
	IPCNode * guarder;
	uint64_t voucher_copy_src;
};

class IPCNode {
	voucher_ev_t *voucher;
	msg_ev_t * msg;
	vector<IPCNode *> children;
	IPCNode *parent;
	uint32_t depth;

public:
	IPCNode(voucher_ev_t *voucher_info) {
		voucher = voucher_info;
		msg = NULL;
		parent = NULL;
		depth = 0;
	}
	void set_msg(msg_ev_t * mach_msg) {
		msg = mach_msg;
	}
	msg_ev_t * get_msg(void) {return msg;}
	voucher_ev_t * get_voucher() {return voucher;}

	void add_child(IPCNode * child) {
		children.push_back(child);
		child->set_parent(this);
	}

	vector<IPCNode *> & get_children() {return children;}

	void set_parent(IPCNode * p) {
		parent = p;
		depth = p->get_depth() + 1;
	}

	IPCNode * get_parent() {return parent;}
	uint32_t get_depth() {return depth;}
};

class IPCTree{
	IPCNode * root;
public:
	IPCTree(IPCNode *_root) {
		root = _root;
	}
	IPCNode * get_root(void) {return root;}
	void decode_tree(IPCNode *root, ofstream & output);
};

class IPCForest{
	map<uint64_t, struct voucher_state> voucher_status;
	map<uint64_t, IPCNode *> incomplete_nodes;
	list<event_t *> evlist;
	vector<IPCTree *> Roots;

public:
	IPCForest(list<event_t *>& evlist);
	void construct_new_node(voucher_ev_t * voucher_info);
	bool update_voucher_and_connect_ipc(IPCNode *ipcnode);
	void construct_ipc_relation(msg_ev_t * mach_msg);
	void update_voucher_status(voucher_conn_ev_t * voucher_copy);
	void update_voucher_status(voucher_transit_ev_t *voucher_transit);
	void construct_forest(void);
	void check_remain_incomplete_nodes(void);
	void decode_voucher_relations(const char * outfile);
	void clear() {//TODOdelete IPCNode;
	}
};

#endif
