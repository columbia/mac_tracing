#ifndef CRITICALPATH_HPP
#define CRITICALPATH_HPP
#include "graph.hpp"

class Element
{
public:
	EventBase *in;
	Node *segment;
	EventBase *out;
	Element(EventBase *in_, Node *segment_, EventBase *out_) {
		in = in_;
		out = out_;
		segment = segment_;
	}
};

class CriticalPath
{
public:
    typedef uint64_t group_id_t;
    typedef std::map<Edge *, group_id_t> edge_map_t;
    typedef std::map<EventBase *, Edge *> event_to_edge_map_t;
	
private:
    Graph *graph;
    Node *root_node;
    Node *end_node;
	std::string output_path;
	std::vector<Element *> detailed_path;
    std::vector<Node *> path;
	std::map<uint64_t, uint64_t> store_weak_edges;
	uint32_t multiple_incoming_edges_node;
    
    static bool node_weight_compare(Node *elem1, Node *elem2);
    static bool edge_weight_compare(Edge *edge1, Edge *edge2);
    edge_map_t in_edges_before_deadline(event_to_edge_map_t, double);
    std::vector<Node *> sort_nodes_from_edges(edge_map_t);
    std::vector<Edge *> sort_in_edges(edge_map_t);
	void print_ident(int ident);
    std::vector<Node *> add_weak_edge(int ident, Node *cur_node, double);
	std::vector<Element *> add_weak_edge(int ident, Node *cur_node, EventBase *);
    std::vector<Node *> critical_path_to(int ident, Node *cur_node, double);
	std::vector<Element *> critical_path_to(int ident, Node *cur_node, EventBase *);
    double calculate_deadline(edge_map_t in_edges, Node *from);

public:
	CriticalPath(Graph *graph_, Node *root_, Node *end_, std::string path_="critical_path.log");
    //CriticalPath(Graph *graph, Node *root, Node *end, std::string output_path);
    ~CriticalPath();

    bool extract_path();
	std::vector<Element *> get_path() {return detailed_path;}
	void save_path(std::ofstream &out);
    void save_path_to_file(std::string filepath);
	void save_path_to_file(std::string filepath, std::vector<Node *> &path);
	void save_path_to_file(std::string filepath, std::vector<Element *> &path);
	void save_weak_edges_to_file(std::string output_path);
};
#endif
