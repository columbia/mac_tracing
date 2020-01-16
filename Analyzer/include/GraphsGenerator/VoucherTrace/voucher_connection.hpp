#ifndef VOUCHER_CONNECTION
#define VOUCHER_CONNECTION
#include "voucher.hpp"
#include "mach_msg.hpp"
#include <vector>

#define HOLD_SEND     1
#define HOLD_RECV    2
#define IN_CREATE    3
#define IN_REUSE    4
#define IN_COPY        5
#define DEALLOC        6

class IPCNode;

struct voucher_state{
    uint64_t holder    : 56,
             status : 8;
    IPCNode * guarder;
    uint64_t voucher_copy_src;
};

class IPCNode {
    VoucherEvent *voucher;
    MsgEvent * msg;
    std::vector<IPCNode *> children;
    IPCNode *parent;
    uint32_t depth;

public:
    IPCNode(VoucherEvent *voucher_info) {
        voucher = voucher_info;
        msg = nullptr;
        parent = nullptr;
        depth = 0;
    }
    void set_msg(MsgEvent * mach_msg) {
        msg = mach_msg;
    }
    MsgEvent * get_msg(void) {return msg;}
    VoucherEvent * get_voucher() {return voucher;}

    void add_child(IPCNode * child) {
        children.push_back(child);
        child->set_parent(this);
    }

    std::vector<IPCNode *> & get_children() {return children;}

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
    void decode_tree(IPCNode *root, std::ofstream & output);
};

class IPCForest{
    std::map<uint64_t, struct voucher_state> voucher_status;
    std::map<uint64_t, IPCNode *> incomplete_nodes;
    std::list<EventBase *> evlist;
    std::vector<IPCTree *> Roots;

public:
    IPCForest(std::list<EventBase *>& evlist);
    void construct_new_node(VoucherEvent * voucher_info);
    bool update_voucher_and_connect_ipc(IPCNode *ipcnode);
    void construct_ipc_relation(MsgEvent * mach_msg);
    void update_voucher_status(VoucherConnectEvent * voucher_copy);
    void update_voucher_status(VoucherTransitEvent *voucher_transit);
    void construct_forest(void);
    void check_remain_incomplete_nodes(void);
    void decode_voucher_relations(const char * outfile);
    void clear() {//TODOdelete IPCNode;
    }
};

#endif
