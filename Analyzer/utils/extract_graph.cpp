#include "extract_graph.hpp"

GraphExtracter::GraphExtracter(Graph *m_graph, EventBase *m_root, EventBase *end_event)
:main_graph(m_graph), root(m_root), end(end_event)
{
    //sub_graph = new Graph();
    std::list<EventBase *> eventlist = slice_with_events(root, end);
    eventlists = new EventLists(eventlist);
    sub_groups_ptr = new Groups(eventlists);
    sub_graph = new Graph(sub_groups_ptr);
}

GraphExtracter::~GraphExtracter()
{
    delete sub_graph;
    delete sub_groups_ptr;
    delete eventlists;
}

bool GraphExtracter::add_node(Node *node)
{
    if (sub_graph->check_and_add_node(node) == nullptr)
        return true;
    else
        return false;
}

bool GraphExtracter::add_edge(Edge *edge)
{
    if (sub_graph->check_and_add_edge(edge) == nullptr)
        return true;
    else
        return false;
}


std::list<EventBase *> GraphExtracter::slice_with_events(EventBase *begin_event, EventBase *end_event)
{
    std::list<EventBase *> eventlist = main_graph->get_event_list();
    std::list<EventBase *>::iterator it;
    it = std::find(eventlist.begin(), eventlist.end(), begin_event);
    assert(it != eventlist.end());
    eventlist.erase(eventlist.begin(), it);
    it = std::find(eventlist.begin(), eventlist.end(), end_event);
    assert(it != eventlist.end());
    it++;
    eventlist.erase(it, eventlist.end());
    
    return eventlist;
}

void GraphExtracter::augment_from_event(EventBase *m_event)
{
    Node *cur_node = main_graph->id_to_node(m_event->get_group_id());
    
    if (cur_node == nullptr) {
        std::cout << __func__ << " 1" << std::endl;
        return;
    }
    //add the node to the container
    if (add_node(cur_node) == false) {
        std::cout << __func__ << " 2" << std::endl;
        return;
    }
    std::cout << "Nodes added # = " << sub_graph->nodes.size() << std::endl;
    /* add out-going edges after m_event into the container
     * iterate peers of the out-going edges
     * and call this function recursively
     */
    std::map<EventBase *, Edge *> outgoing_edges = cur_node->get_out_edges();
    std::map<EventBase *, Edge *>::iterator it;
    std::cout << "Outgoing edges = " << outgoing_edges.size() << std::endl;
    for (it = outgoing_edges.begin(); it != outgoing_edges.end(); it++) {
        if (it->first->get_abstime() <= m_event->get_abstime())
            continue;
        if (add_edge(it->second) == false)
            continue;
        if (it->second->to == cur_node)
            continue;
        std::cout << __func__ << " 3" << std::endl;
        augment_from_event(it->second->e_to);
    }
    
    if (outgoing_edges.size() == 0) {
    }
}

void GraphExtracter::streamout_events(std::string path)
{
    std::ofstream dump(path);
    if (dump.fail()) {
        std::cout << "unable to open file " << path << std::endl;
        exit(EXIT_FAILURE);
    }

    /*
    std::list<EventBase *>::iterator it;
    for(it = eventlist.begin(); it != eventlist.end(); it++) {
        EventBase *cur_ev = *it;
        cur_ev->streamout_event(dump);
    }
    dump.close();
    */
}
