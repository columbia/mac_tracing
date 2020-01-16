#ifndef SEARCH_GRAPH_HPP
#define SEARCH_GRAPH_HPP
#include "graph.hpp"
#include "canonization.hpp"
#include <sstream>


class GraphSearcher {
    Graph *graph;
    Node *buggy_node;
    std::map<EventType::event_type_t, bool> key_events;
    std::map<tid_t, std::vector<NormNode*> >tid_normnodes_map; 
    std::vector<Node *> similar_nodes;
    std::vector<Node *> path;
    std::vector<uint64_t> weak_edge_node_ids;
    std::vector<uint64_t> multi_edge_node_ids;
    
private:
    std::map<EventType::event_type_t, bool> load_default_key_events();
    std::vector<NormNode *> update_normnodes_for_thread(tid_t thread);
    NormNode *norm_node_fast(Node *node);

    std::vector<Node *> get_similar_nodes_in_thread_before(Node *node);
    std::vector<Node *> get_similar_nodes_in_thread_before(Node *node, int number);
    std::vector<Node *> get_similar_nodes_in_thread_after(Node *node, double deadline);
    std::vector<Node *> get_baseline_on_syscall(Node *node); 
    std::vector<Node *> get_baseline_on_timespan(Node *node);
    std::vector<Node *> get_baseline_on_waitcost(Node *node); 
    std::vector<Node *> search_baseline_nodes(Node *spinning_node);
    bool compare_node(Node *cur_normal_node, Node *potential_buggy_node);


    void check_and_remove_from_weak_edge_node_ids(uint64_t gid);
    void check_and_remove_from_multi_edge_node_ids(uint64_t gid);

    static bool cmp_edge_by_intime(Edge *e1, Edge *e2);
    std::list<Edge *>get_incomming_edges_before(std::map<EventBase *, Edge*> &in_edges, double deadline);
    Node *get_prev_node_in_thread(Node *node, Node **strong_node);
    Node *prev_node_from_weak_edge(Node *node);
    uint64_t path_slice_from_node(Node *node, double deadline = -1.0);

    void print_nodes(std::vector<Node *> t_nodes);
    void print_incoming_edges(std::list<Edge *> incoming_edges);
    std::map<uint64_t, EventBase *> collapse_prevs(std::list<Edge *> incoming_edges);

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
    std::vector<Node *> suspicious_nodes;

    Node *id_to_node(uint64_t group_id) {return graph->id_to_node(group_id);}
    Node *spinning_node();
    int spinning_type(Node *spinning_node);
    bool is_wait_spinning();
    std::string decode_spinning_type(int);
    std::string decode_spinning_type();
    Node *get_begin_node_for_causal_path_slicing(Node *node);

    //void js_paths(std::vector<Node *>& paths, Node *node, std::string file_path);
    void clear_path() {path.clear(); weak_edge_node_ids.clear(); multi_edge_node_ids.clear();}
    bool clear_path_from_node(uint64_t gid);//{/*find the node from path and clear it until the last node in path*/}
    uint64_t continue_backward_slice(uint64_t gid, int event_index);
    uint64_t init_diagnose();
    int path_comparison();
    void get_culprits();
    void js_paths(std::vector<Node *>& paths, Node *node, std::string file_path);
    void show_path();
};
#endif

