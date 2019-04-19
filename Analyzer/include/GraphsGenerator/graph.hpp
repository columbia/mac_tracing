#ifndef GRAPH_HPP
#define GRAPH_HPP
#include "group.hpp"

#define MSGP_REL		0
#define MKRUN_REL	 	1
#define WAIT_REL		2
#define DISP_EXE_REL 		3
#define DISP_DEQ_REL		4
#define TIMERCALLOUT_REL	5
#define TIMERCANCEL_REL 6
#define CA_REL		7
#define BRTRAP_REL	8
#define RLITEM_REL	9

class Edge;
class Node;
class Graph;

class Edge{
public:
	//group_t *g_from;
	//group_t *g_to;
	Node *from;
	Node *to;
	event_t *e_from;
	event_t *e_to;
	uint32_t rel_type;

	Edge(event_t *, event_t*, uint32_t);
	~Edge();
	bool operator==(const Edge &edge) const
	{
		return (e_from == edge.e_from 
			&& e_to == edge.e_to 
			&& rel_type == edge.rel_type);
	}
};

class Node{
	Graph *parent;
	group_t *group;
	map<event_t *, Edge *> in_edges; 
	map<event_t *, Edge *> out_edges;

	bool add_in_edge(Edge *e);
	bool add_out_edge(Edge *e);

	void add_edge_for_msg(msg_ev_t *);

	void add_edge_for_disp_enq(enqueue_ev_t *);
	void add_edge_for_disp_deq(dequeue_ev_t *);

	void add_edge_for_callcreate(timercreate_ev_t *);
	void add_edge_for_callout(timercallout_ev_t *timer_callout);
	void add_edge_for_callcancel(timercancel_ev_t *timer_callout);

	void add_edge_for_caset(ca_set_ev_t *);
	void add_edge_for_cadisplay(ca_disp_ev_t *ca_display_event);

	void add_edge_for_rlitem(rl_boundary_ev_t *);
	void add_edge_for_hwbr(breakpoint_trap_ev_t *event);
	void add_edge_for_mkrun(mkrun_ev_t *);
	void add_edge_for_fakedwoken(FakedwokenEvent *);

	void add_edge_for_wait(wait_ev_t *);

	bool find_edge_in_map(pair<event_t *, Edge *> target, bool out_edge);
	//bool find_edge_in_map(Edge *target, bool out_edge);
public:
	Node(Graph *, group_t *);
	~Node();
	map<event_t *, Edge *> &get_in_edges();
	map<event_t *, Edge *> &get_out_edges();
	group_id_t get_gid() {return group->get_group_id();}
	tid_t get_tid() {return group->get_tid();}
	group_t *get_group(){return group;}
	
	void add_edges();
	void remove_edges();
	void remove_edge(pair<event_t *, Edge *> edge, bool out_edge);
	//void remove_edge(Edge *, bool out_edge);
	int compare_syscall_ret(Node *peer);
	int compare_timespan(Node *peer);
	int compare_wait_cost(Node *peer);
	double get_begin_time();
	double get_end_time();
	bool contains_noncausual_mk_edge();
	bool wait_over();
	void pattern_check();
};

class Graph
{
	groups_t *groups_ptr;
	mutex graph_edge_mtx;
public:
	map<group_t *, Node *> nodes_map;
	map<tid_t, list<Node *> >tid_glist_map;
	vector<Node *> nodes;
	map<event_t *, Edge *> edge_map;
	vector<Edge *> edges;

	Graph(groups_t *groups);
	~Graph();
	void init_graph();
	void remove_node(Node *node);
	void remove_edge(Edge *e);
	Edge *check_and_add_edge(Edge *edge);
	void add_edge(Edge *edge);
	vector<Node *> get_nodes_for_tid(tid_t tid);
	vector<Node *> get_nodes_in_thread_after(Node *cur_node);
	Node *id_to_node(group_id_t gid);
	Node *get_spinning_node();
	void streamout_nodes_and_edges(string path);
	void check_pattern_for_mkrun(string path);
};
#endif
