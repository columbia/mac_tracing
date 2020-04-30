#ifndef TRACE_FOR_LEARN_HPP
#define TRACE_FOR_LEARN_HPP
#include "graph.hpp"
#include "canonization.hpp"
#include "search_graph.hpp"

class TraceForLearn {
    Graph *graph;
	GraphSearcher *search_engine;
	std::vector<Node *> nodes;
	std::vector<Edge *> edges;
	
	std::map<Edge *, bool> inspect_edges;
	std::map<Edge *, std::vector<Edge *> > edge_categories;

public:
	TraceForLearn(Graph *_graph, GraphSearcher *_searcher){
		graph = _graph;
		search_engine = _searcher;
		graph->generate_tfl_list();

		nodes = graph->get_nodes();
		edges = graph->get_edges();	
		inspect_edges.clear();
	}

	std::map<Edge *, bool>  &collect_inspect_edges();
	void divide_edges();
	void examine_edge(int index);
	Edge *hueristics_examine(Node *to, int n);
	bool similar(Edge *edge1, Edge *edge2);
	void print_edge(Edge *edge, std::ofstream &out);
	bool validate_edge(int index, bool valid);
	int propagate_similar_edges(Edge *edge, bool val);
	void tfl_pair(Node *from, Node *to, std::ofstream &out);
	void tfl_pair_to_file(uint64_t from, uint64_t to, std::string path);
	void tfl_edge_sequences(std::string out);

	void tfl_edge_positive(std::string path, uint64_t *max_patterns);
	void tfl_edge_negtive(std::string path, uint64_t max_patterns);

	void tfl_edges(std::set<Edge *>, std::string path);
    
};

#endif
