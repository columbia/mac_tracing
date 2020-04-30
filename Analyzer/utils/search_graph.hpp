#ifndef SEARCH_GRAPH_HPP
#define SEARCH_GRAPH_HPP
#include "graph.hpp"
#include "canonization.hpp"
#include <sstream>


class GraphSearcher {
    Graph *graph;
    Node *anomaly_node;

    std::map<EventType::event_type_t, bool> key_events;
    std::map<tid_t, std::vector<NormNode*> >tid_normnodes_map; 
    std::vector<Node *> similar_nodes;
    std::vector<Node *> path;
    std::vector<uint64_t> weak_edge_node_ids;
    std::vector<uint64_t> multi_edge_node_ids;
    std::vector<Node *> suspicious_nodes;
	std::map<Node *, int> cached_selection;
	Node *stopping_node;
    
private:
    std::map<EventType::event_type_t, bool> load_default_key_events();
    std::vector<NormNode *> update_normnodes_for_thread(tid_t thread);
    NormNode *norm_node_fast(Node *node);

    std::vector<Node *> get_prev_similar_nodes(Node *node);
    std::vector<Node *> get_prev_similar_nodes(Node *node, int number);
    std::vector<Node *> get_post_similar_nodes(Node *node, double deadline);
    std::vector<Node *> get_baseline_on_syscall(Node *node); 
    std::vector<Node *> get_baseline_on_timespan(Node *node);
    std::vector<Node *> get_baseline_on_waitcost(Node *node); 
    //bool compare_node(Node *normal_node, Node *node);
	int select_base_on_cached_node(Node *cur_node);

    void check_and_remove_from_weak_edge_node_ids(uint64_t gid);
    void check_and_remove_from_multi_edge_node_ids(uint64_t gid);

    static bool cmp_edge_by_intime(Edge *e1, Edge *e2);
	std::vector<Edge *> get_incomming_edges_before(Node *cur_node, double deadline);
	std::vector<Edge *> get_strong_edges_before(Node *cur_node, double deadline);
	std::vector<Edge *> get_incomming_edges(Node *cur_node);
	std::vector<Edge *> get_strong_edges(Node *cur_node);
    Node *get_prev_node_in_thread(Node *node, Node **strong_node);
    Node *prev_node_from_weak_edge(Node *node);
    uint64_t path_slice_from_node(Node *node, double deadline = -1.0);
	uint64_t check_converge(Node *, Node *, int);

    void print_nodes(std::vector<Node *> t_nodes);
    void print_incoming_edges(std::vector<Edge *> incoming_edges);
    std::map<uint64_t, EventBase *> collapse_prevs(std::vector<Edge *> incoming_edges);

	void tfl_pair(Node *from, Node *to, std::ofstream &out);
	void tfl_selection(uint64_t from, uint64_t to, int type);
	void record_positive(uint64_t from, uint64_t to);
	void record_na(uint64_t from, uint64_t to);
	void record_negtive(uint64_t from, uint64_t to);

public:
    static const int SPINNING_NONE = -1;
    static const int SPINNING_BUSY = 0;
    static const int SPINNING_YIELD = 1;
    static const int SPINNING_WAIT = 2;
    static const int COMPARE_SYSCALL_RET = 0;
    static const int COMPARE_NODE_TIME = 1;
    static const int COMPARE_WAIT_TIME = 2;

    GraphSearcher(Graph *_graph, std::map<EventType::event_type_t, bool> event_for_compare);
    GraphSearcher(Graph *_graph);
    ~GraphSearcher();

	Edge *refine_edge(Node *to);
	Edge *heuristic_edge(Node *to, int n);

    Node *id_to_node(uint64_t group_id) {return graph->id_to_node(group_id);}

    Node *get_anomaly_node();
	bool set_anomaly_node(uint64_t gid);
	void clear_anomaly_node() {anomaly_node = nullptr;}
    int spinning_type(Node *spinning_node);
    bool is_wait_spinning();
    std::string decode_spinning_type(int);
    std::string decode_spinning_type();
	void show_similar_nodes(uint64_t gid);

	uint64_t check_converge(uint64_t gid_1, uint64_t gid_2, int num);
    void clear_path();
    bool clear_path_from_node(uint64_t gid);
    void js_paths(std::vector<Node *>& paths, Node *node, std::string file_path);

    std::vector<Node *> search_baseline_nodes(Node *spinning_node);
	uint64_t search_from_baseline(uint64_t gid);
    uint64_t continue_backward_slice(uint64_t gid, int event_index);

    uint64_t init_diagnose();
    int path_comparison();
    void show_path();
    void get_culprits();
	void record_selection();
};
#endif

