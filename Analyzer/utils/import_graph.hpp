#ifndef IMPORT_GRAPH_HPP
#define IMPORT_GRAPH_HPP
#include "graph.hpp"

class ImportedGraph: public Graph{
    std::list<EventBase *> event_list;
    Graph *original_graph;

public:
    ImportedGraph(Graph *_original_graph, std::string path);
    Node *event_to_node(EventBase *);
    EventBase *seq_to_event(uint64_t id);
    void import_edges(std::string graph_path);
    void compare_nodes();
};

#endif
