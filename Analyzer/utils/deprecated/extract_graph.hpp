#ifndef EXTRACT_GRAPH_HPP
#define EXTRACT_GRAPH_HPP
#include "graph.hpp"
#include "canonization.hpp"

class GraphExtracter {
    Graph *main_graph;
    Graph *sub_graph;
    EventLists *eventlists;
    Groups *sub_groups_ptr;
    EventBase *root;
    EventBase *end;
    bool add_node(Node *node);
    bool add_edge(Edge *edge);
    void augment_from_event(EventBase *m_event);
    std::list<EventBase *> slice_with_events(EventBase *m_event, EventBase *end_event);
public:
    GraphExtracter(Graph *m_graph, EventBase *m_root, EventBase *m_end);
    ~GraphExtracter();
    Graph *get_sub_graph(void) {return sub_graph;}
    void streamout_events(std::string path);
};

#endif
