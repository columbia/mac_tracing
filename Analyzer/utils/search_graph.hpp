#ifndef SEARCH_GRAPH_HPP
#define SEARCH_GRAPH_HPP
#include "graph.hpp"
#include "canonization.hpp"

class GraphSearcher {
	Graph *graph;
	map<event_id_t, bool> key_events;
	map<tid_t, vector<NormNode*> >tid_normnodes_map; 
	
	vector<NormNode *> update_normnodes_for_thread(tid_t thread);
	NormNode *norm_node_fast(Node *node);

public:
	GraphSearcher(Graph *_graph, map<event_id_t, bool> event_for_compare);
	~GraphSearcher();
	map<event_id_t, bool> load_default_key_events();
	Node *spinning_node();
	vector<Node *> get_similar_nodes_in_thread_before(Node *node);
	vector<Node *> get_similar_nodes_in_thread_before(Node *node, int number);
	vector<Node *> get_similar_nodes_in_thread_after(Node *node, double deadline);
	vector<Node *> get_counterpart_on_syscall(Node *node, vector<Node *> similar_nodes);
	vector<Node *> get_counterpart_on_timespan(Node *node, vector<Node *> similar_nodes);
	vector<Node *> get_counterpart_on_waitcost(Node *node, vector<Node *>similar_nodes);

	vector<Node *> wakeup_node(Node *counter_node);
	bool compare_node(Node *cur_normal_node, Node *potential_buggy_node);
	bool compare_prev_step_for_nodes(Node *target_node, Node *peer_node);
	vector<Node *> get_peers(map<event_t *, Edge*> &in_edges);
	Node *get_prev_node_in_thread(Node *node);
	vector<Node *> slice_path_for_node(Node *, bool interference);
	void examine_path_backward(Node *buggy_node, vector<Node *> normal_path_for_compare, bool interference);
	void plot_node();//gnu_plot
	void plot_nodes();
	
};
#endif

