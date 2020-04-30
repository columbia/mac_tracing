#include "import_graph.hpp"
#include <sstream>

ImportedGraph::ImportedGraph(Graph *_original_graph, std::string path)
:Graph(_original_graph->get_groups_ptr())
{
    event_list = _original_graph->get_event_list();
    original_graph = _original_graph;
    import_edges(path);
}

Node *ImportedGraph::event_to_node(EventBase *event)
{
    Group *group = groups_ptr->group_of(event);
    Node *new_node = new Node(this, group);
    Node *old_node = check_and_add_node(new_node);
    if (old_node) {
        delete new_node;
        return old_node;
    }
    return new_node;
}

EventBase *ImportedGraph::seq_to_event(uint64_t id)
{
    std::list<EventBase *>::iterator it = event_list.begin();
    if (id >= event_list.size())
        return nullptr;
    std::advance(it, id);
    return *it;
}

void ImportedGraph::import_edges(std::string graph_path)
{
    std::ifstream infile(graph_path);
    std::string line;
    uint64_t id1, id2;
    std::string delimiter;

    //EventBase *event_from, *event_to;
    //Group *group_from, *group_to;

    EventBase *e_from, *e_to;
    Node *from, *to;
    while (getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> id1 >> delimiter >> id2))
            continue;
        e_from = seq_to_event(id1);
        e_to = seq_to_event(id2);
        if (!e_from || !e_to) {
            mtx.lock();
            std::cout << "invalid pair" << id1 << "-->" << id2 << std::endl;
            mtx.unlock();
            continue;
        }
        from = event_to_node(e_from);
        to = event_to_node(e_to);
        if (!from || !to) {
            mtx.lock();
            std::cout << "invalid node in " << id1 << "-->" << id2 << std::endl;
            mtx.unlock();
            continue;
        }

        //Edge *new_edge =  new Edge(e_from , e_to, IMPORT_REL);
        //if (!from->add_out_edge(new_edge))
            //delete new_edge;
		//auto edges = from->get_out_edges();
       // to->add_in_edge(from->get_out_edges()[e_from]);
    }
}

void ImportedGraph::compare_nodes()
{

}

