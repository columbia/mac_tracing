#ifndef CRITICALPATH_HPP
#define CRITICALPATH_HPP
#include "graph.hpp"

class Element
{
    Edge *edge;
	EventBase *in;
	Node *segment;
	EventBase *out;
    bool weak;

public:
	Element(Edge *in_edge_, EventBase *in_, Node *segment_, EventBase *out_, bool weak_) {
        edge = in_edge_;
		in = in_;
		out = out_;
		segment = segment_;
        weak = weak_;       
	}

    bool is_weak() {return weak;}
    Node *node() {return segment;}
    EventBase *begin() {return in;}
    EventBase *end() {return out;}
    Edge *in_edge() {return edge;}
};

class CriticalPath
{
public:
    typedef uint64_t group_id_t;
    typedef std::map<Edge *, group_id_t> edge_sink_map_t;
	typedef Graph::edge_map_t edge_map_t;
	
    CriticalPath(Graph *graph_, Node *root_, Node *end_, EventBase *root_ev_, EventBase *end_ev_, std::string path_ = "critical_path.log");
	CriticalPath(Graph *graph_, Node *root_, Node *end_, std::string path_="critical_path.log");
	CriticalPath(Graph *graph_, uint64_t root, uint64_t end, std::string path_="critical_path.log");
    ~CriticalPath();

	bool check_reachable() {return reachable;}
	std::vector<Element *> &get_path() {return detailed_path;}
	void append_path_to_file(std::string filepath, std::vector<Element *> &path);

private:
    Graph *graph;
    Node *root_node;
    Node *end_node;
    EventBase *root_ev;
    EventBase *end_ev;
	bool reachable;
	std::vector<Element *> detailed_path;

    std::unordered_map<Node *, bool> visited;
    
    static bool node_weight_compare(Node *elem1, Node *elem2);
    static bool edge_weight_compare(Edge *edge1, Edge *edge2);

    edge_sink_map_t filt_with_deadline(edge_map_t, double);
    std::vector<Node *> get_sorted_nodes(edge_sink_map_t);
    std::vector<Edge *> sort_edges(edge_sink_map_t);

    std::vector<Edge *> filter_with_deadline(edge_map_t, double);
	void print_ident(int ident);
    bool valid(int ident, Node *cur_node, Edge *edge);
	std::vector<Element *> critical_path_to_root(int ident, Node *cur_node, Edge*);
    bool extract_path();
};
#endif
