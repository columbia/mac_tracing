#ifndef VERTICESCLASS_HPP
#define VERTICESCLASS_HPP
#include "graph.hpp"
class VerticesClass {
	Graph *graph;
	std::map<std::string, std::vector<uint64_t> > vertice_map;
public:
	VerticesClass(Graph *_graph);
	uint64_t classify_vertices();
	void render_statistics();
};
#endif
