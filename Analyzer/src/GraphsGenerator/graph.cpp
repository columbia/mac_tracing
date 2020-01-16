#include "graph.hpp"
#include <math.h>
#include <boost/filesystem.hpp>

Edge::Edge(EventBase *e1, EventBase *e2, uint32_t rel)
{
    e_from = e1;
    e_to = e2;
    rel_type = rel;
    from = to = nullptr;
    assert(e_from && e_to);
    weight = e_to->get_abstime() - e_from->get_abstime();
}

Edge::~Edge()
{
    from = to = nullptr;
    e_from = e_to = nullptr;
}
//////////////////////////////////

Node::Node(Graph *_p, Group *_g)
:parent(_p), group(_g)
{
    out_edges.clear();
    in_edges.clear();
    in_weak_edges.clear();
    out_weak_edges.clear();
    in = out = 0;
    weight = get_end_time() - get_begin_time();
}


Node::~Node()
{
    parent = nullptr;
    group  = nullptr;
}

bool Node::add_in_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
        in_edges[e->e_to] = e;
		e->set_edge_to_node(this);
		inc_in();
    } else {
        in_edges[e->e_to] = exist_edge;
		exist_edge->set_edge_to_node(this);
    }
    return ret;
}

bool Node::add_in_weak_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
        in_weak_edges[e->e_to] = e;
		e->set_edge_to_node(this);
		inc_weak_in();
    } else {
        in_weak_edges[e->e_to] = exist_edge;
		exist_edge->set_edge_to_node(this);
    }
    return ret;
}

bool Node::add_out_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
        out_edges[e->e_from] = e;
		e->set_edge_from_node(this);
		inc_out();
    } else {
        out_edges[e->e_from] = exist_edge;
		exist_edge->set_edge_from_node(this);
    }
    return ret;
}

bool Node::add_out_weak_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
        out_weak_edges[e->e_from] = e;
        e->set_edge_from_node(this);
		inc_weak_out();
    } else {
        out_weak_edges[e->e_from] = exist_edge;
        exist_edge->set_edge_from_node(this);
    }
    return ret;
}

std::map<EventBase *, Edge *> &Node::get_in_edges()
{
    return in_edges;
}

std::map<EventBase *, Edge *> &Node::get_out_edges()
{
    return out_edges;
}

void Node::remove_edges()
{
    while (in_edges.size() > 0) {
        assert(in_edges.begin() != in_edges.end());
        std::pair<EventBase *, Edge*> edge = *(in_edges.begin());
        remove_edge(edge, false);
    }
    while (out_edges.size() > 0) {
        assert(out_edges.begin() != out_edges.end());
        std::pair<EventBase *, Edge*> edge = *(out_edges.begin());
        remove_edge(edge, true);
    }
    assert(in_edges.size() == 0 && out_edges.size() == 0);
}

bool Node::find_edge_in_map(std::pair<EventBase *, Edge *> target, bool out_edge)
{
    std::map<EventBase *, Edge *> &map = out_edge ? out_edges: in_edges;
    assert(map.size() == out_edges.size() || map.size() == in_edges.size());
    
    if (map.find(target.first) != map.end()) {
        return true;
    }
    return false;
}

void Node::remove_edge(std::pair<EventBase *, Edge *> edge, bool out_edge)
{
    assert(edge.first != nullptr && edge.second != nullptr);
    assert(edge.second->e_from == edge.first || edge.second->e_to == edge.first);
    if (find_edge_in_map(edge, out_edge) == false) {
        return;
    }
    Edge *e = edge.second;
    
    if (e->from == e->to) {
        in_edges.erase(e->e_to);
        out_edges.erase(e->e_from);
    }
    else if (out_edge) {
        Node *peer = e->to;
        if (peer) {
            out_edges.erase(e->e_from);
            peer->remove_edge(std::make_pair(e->e_to, e), false);
        }
    } else {
        Node *peer = e->from;
        if (peer) {
            in_edges.erase(e->e_to);
            peer->remove_edge(std::make_pair(e->e_from, e), true);
        }
    }
    parent->remove_edge(e);
}

int Node::compare_syscall_ret(Node *peer)
{
    return get_group()->compare_syscall_ret(peer->get_group());
}

int Node::compare_timespan(Node *peer)
{
    return get_group()->compare_timespan(peer->get_group());
}

int Node::compare_wait_cost(Node *peer)
{
    return get_group()->compare_wait(peer->get_group());
}

double Node::get_begin_time()
{
    return group->get_first_event()->get_abstime();
}

double Node::get_end_time()
{
    EventBase *last_event = group->get_last_event();
    if (last_event->get_event_type() == WAIT_EVENT) {
        WaitEvent *wait = dynamic_cast<WaitEvent *>(last_event);
        MakeRunEvent* mkrun = dynamic_cast<MakeRunEvent *>(wait->get_event_peer());
        if (mkrun && mkrun->is_external())
            return mkrun->get_abstime();       
    }
    return last_event->get_abstime();
}

bool Node::contains_nsapp_event()
{
    return group->contains_nsappevent() != nullptr;
}

bool Node::contains_view_update()
{
    return group->contains_view_update() != nullptr;
}

bool Node::wait_over()
{
    return group->wait_over();
}

Node *Node::prev_weak_in_thread()
{
    gid_t search_gid = get_gid();
    Node *search_node = nullptr;

    while (search_node == nullptr) {
        search_node = parent->id_to_node(--search_gid);
        if (search_gid < group->get_group_id() - 10 || search_gid <= (group->get_group_id() & ~(0xfffffULL) ))
            break;
    }

    return search_node;
}

EventBase *Node::index_to_event(int index)
{
    std::list<EventBase *> &container = group->get_container();
    std::list<EventBase *>::iterator it = container.begin();

    if (index >= container.size() || index < 0)
        return nullptr;

    std::advance(it, index);
    return *it;    
}
///////////////////////////////////
std::string Graph::create_output_path(std::string input_path)
{
    boost::filesystem::path outdir("./output/");
    if (!(boost::filesystem::exists(outdir)))  
        boost::filesystem::create_directory(outdir);

    std::string filename;
    size_t dir_pos = input_path.find_last_of("/");
    if (dir_pos != std::string::npos) {
        filename = input_path.substr(dir_pos + 1);
    } else {
        filename = input_path;
    }

    size_t pos = filename.find(".");
    if (pos != std::string::npos)
        return outdir.c_str() + filename.substr(0, pos);
    else
        return outdir.c_str() + filename;
}

Graph::Graph()
{
    std::string out_file = create_output_path(LoadData::meta_data.data);
    std::string event_output_path = out_file + "_events.stream";
    std::string group_output_path = out_file + "_groups.stream";

    event_lists_ptr = new EventLists(LoadData::meta_data);
    groups_ptr = new Groups(event_lists_ptr);

    event_lists_ptr->streamout_all_event(event_output_path);
    groups_ptr->streamout_groups(group_output_path);
    construct_inside = true;
    create_graph = true;

	edges.clear();
	nodes.clear();
    nodes_map.clear();
    edge_map.clear();
}

Graph::Graph(Groups *_groups_ptr)
{
    event_lists_ptr = nullptr;
    groups_ptr = _groups_ptr;
    construct_inside = false;
    create_graph = true;

	edges.clear();
	nodes.clear();
    nodes_map.clear();
    edge_map.clear();
}

Graph::~Graph(void)
{
    nodes_map.clear();
    edge_map.clear();

    if (create_graph) {
        std::vector<Node *>::iterator node_it;
        for (node_it = nodes.begin(); node_it != nodes.end(); node_it++) {
            Node *node = *(node_it);
            assert(node);
            delete(node);
        }

        std::vector<Edge *>::iterator edge_it;
        for (edge_it = edges.begin(); edge_it != edges.end(); edge_it++) {
            Edge *edge = *(edge_it);
            assert(edge);
            delete(edge);
        }
    }

    if (construct_inside) {
        assert(groups_ptr && event_lists_ptr);
        delete groups_ptr;
        delete event_lists_ptr;
    }

    nodes.clear();
    edges.clear();
}

void Graph::remove_node(Node *node)
{
    assert(node != nullptr);
    node->remove_edges();
    std::vector<Node *>::iterator it = find(nodes.begin(), nodes.end(), node);
    assert(it != nodes.end() && *it == node);
    nodes.erase(it);
    delete node;
}

void Graph::remove_edge(Edge *edge)
{
    assert(edge != nullptr);
    std::vector<Edge *>::iterator it = find(edges.begin(), edges.end(), edge);
    if (it != edges.end()) {
        edges.erase(it);
        delete edge;
    }
}

Node *Graph::check_and_add_node(Node *node)
{
    Node *ret = nullptr;
	//check if the node of the same group exists.
	if (nodes_map.find(node->get_group()) != nodes_map.end()) {
        ret = nodes_map[node->get_group()];
    } else {
        nodes.push_back(node);
		nodes_map[node->get_group()] = node;
    }
    return ret;
}

Edge *Graph::check_and_add_edge(Edge *e)
{
    Edge *ret = nullptr;
    graph_edge_mtx.lock();
    if (edge_map.find(e->e_from) != edge_map.end()) {
        //assert(edge_map[e->e_from]->e_to == e->e_to);
        if (edge_map[e->e_from]->e_to == e->e_to) {
            ret = edge_map[e->e_from];
            graph_edge_mtx.unlock();
            return ret;
        } else {
            //notify cases of multiple outgoing edges type : wait event, woken (connected with wake up and faked woken)
            std::cerr << "Same edge from end at " << std::fixed << std::setprecision(1)\
                    << e->e_from->get_abstime() << " with to end ";
			std::cerr << std::fixed << std::setprecision(1) << e->e_to->get_abstime() << " and ";
			std::cerr << edge_map[e->e_from]->e_to->get_abstime() << std::endl;
        }
    }
    //if no the same begin and end, add new edge
    edge_map[e->e_from] = e;
    edges.push_back(e);
    graph_edge_mtx.unlock();
    return ret;
} 

Node *Graph::id_to_node(group_id_t gid)
{
    Group *id_to_group = groups_ptr->get_group_by_gid(gid);
    if (id_to_group == nullptr) {
        std::cerr << "Unknown group id 0x" << std::hex << gid << std::endl;
        return nullptr;
    }

    if (nodes_map.find(id_to_group) != nodes_map.end())
        return nodes_map[id_to_group];
    
    std::cerr << "Node 0x" << std::hex << gid << " not included in graph" << std::endl;
    return nullptr;
}

std::vector<Node *> Graph::get_nodes_for_tid(tid_t tid)
{
    std::vector<Node *> result;
    Groups::gid_group_map_t tgroups = groups_ptr->get_groups_by_tid(tid);
    Groups::gid_group_map_t::iterator it;

    result.clear();
    for (it = tgroups.begin(); it != tgroups.end(); it++) {
        assert(nodes_map.find(it->second) != nodes_map.end());
        result.push_back(nodes_map[it->second]);
    }
    return result;
}

std::vector<Node *> Graph::get_buggy_node_candidates(Node *cur_node)
{
    Node *spinning_node = get_spinning_node();
    std::vector<Node *> ret = get_nodes_for_tid(cur_node->get_tid());
    std::vector<Node *>::iterator it = find(ret.begin(), ret.end(), cur_node);
    ret.erase(ret.begin(), it);
    for (it = ret.begin(); it != ret.end(); it++)
        if ((*it)->get_group()->get_first_event()->get_abstime() 
            > spinning_node->get_group()->get_first_event()->get_abstime())
            break;
    ret.erase(it, ret.end());
    return ret;
}

Node *Graph::node_of(EventBase *event)
{
	Group *group = groups_ptr->group_of(event);
	if (group == nullptr) return nullptr;
	if (nodes_map.find(group) != nodes_map.end())
		return nodes_map[group];
	return nullptr;
}

Group *Graph::group_of(EventBase *event)
{
	if (groups_ptr == nullptr) return nullptr;
	return groups_ptr->group_of(event);
}

Node *Graph::get_spinning_node()
{
    Group *spinning_group = groups_ptr->spinning_group();
    if (spinning_group == nullptr)
        return nullptr;
    if (nodes_map.find(spinning_group) != nodes_map.end()) {
        assert(spinning_group);
        assert(nodes_map[spinning_group]);
        return nodes_map[spinning_group];
    }
    return nullptr;
} 

void Graph::graph_statistics()
{
    std::cout << "Graph size: " << std::endl;
    std::cout << "Edges numer = " << std::dec << edges.size() << std::endl;
    std::cout << "Nodes numer = " << std::dec << nodes.size() << std::endl;

    std::map<uint32_t, uint32_t> edge_categories;
    std::set<std::string> procs_set;
    std::vector<uint64_t> multin_grs; 

    edge_categories.clear();
    procs_set.clear();
    multin_grs.clear();

    std::vector<Edge *>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
        Edge *cur_edge = *e_it;
        edge_categories[cur_edge->rel_type] = edge_categories[cur_edge->rel_type] + 1;
    }
    std::vector<Node *>::iterator it;
    for (it = nodes.begin(); it != nodes.end(); it++) {
        Node *node = *it;
        assert(node->get_group());
        if (node->get_in() > 1)
            multin_grs.push_back(node->get_gid());
        procs_set.insert(node->get_group()->get_first_event()->get_procname());
    }

    std::map<uint32_t, uint32_t>::iterator map_it;
    for (map_it = edge_categories.begin(); map_it != edge_categories.end(); map_it++)
        std::cout << "Edge Category[" << map_it->first << "] = " << std::dec << map_it->second << std::endl;

	std::cout << "number of multiple-incoming-edge nodes = " << std::dec << multin_grs.size() << std::endl; 
    std::vector<uint64_t>::iterator mult_it;
    for (mult_it = multin_grs.begin(); mult_it != multin_grs.end(); mult_it++)
        std::cout << "\t0x" << std::hex << *mult_it;
    std::cout << std::endl;

	std::cout << "number of procs = " << std::dec <<  procs_set.size() << std::endl; 
    std::set<std::string>::iterator proc_it;
    for (proc_it = procs_set.begin(); proc_it != procs_set.end(); proc_it++)
        std::cout << "\t" << *(proc_it);
    std::cout << std::endl;
}

void Graph::tfl_nodes(std::string path) {
    std::string event_path(path + "_events");
    std::list<EventBase *> event_list;

    std::vector<Node *>::iterator it;
    for (it = nodes.begin(); it != nodes.end(); it++) {
        Group *cur_group = (*it)->get_group();
        assert(cur_group);
        std::list<EventBase *> &container = cur_group->get_container();
        event_list.insert(event_list.end(), container.begin(), container.end());
    }
    event_list.sort(Parse::EventComparator::compare_time);
    /*
    std::list<EventBase *>::iterator lit;
    uint32_t index = 0;
    for (lit = event_list.begin(); lit != event_list.end(); lit++) {
        (*lit)->tfl_index = index;
        ++index; 
    }
    */
    EventLists tfl_lists(event_list);
    tfl_lists.tfl_all_event(event_path);
}

void Graph::tfl_edges_between(uint64_t from, uint64_t to, std::string in, std::string out) {
	std::ifstream weak_edge(in);
	std::ofstream output_edge(out);
	uint64_t edge_from, edge_to;
	for (auto cur_edge: edges) {
		edge_from = cur_edge->e_from->get_tfl_index();
		edge_to = cur_edge->e_to->get_tfl_index();
		if (edge_to == 0) {
			//faked woken
			EventBase *real_event = cur_edge->to->get_group()->get_real_first_event();
			assert(real_event);
			edge_to = real_event->get_tfl_index();
		}

		if (edge_from > from && edge_from < to)
			output_edge << std::dec << edge_from << "\t" << edge_to << std::endl;
	}
	output_edge << "Weak Edges derived from reachable path" << std::endl;
	std::string line;
	while(getline(weak_edge, line)) {
		std::istringstream iss(line);
		if (!(iss >> edge_from >> edge_to))
			break;
		if (edge_from > from && edge_from < to)
			output_edge << std::dec << edge_from << "\t" << edge_to << std::endl;
	}
	weak_edge.close();
	output_edge.close();
}

void Graph::tfl_edges(std::string path) {
    std::string edge_path(path + "_edges");
    std::ofstream output_edge(edge_path);

	std::vector<Edge *>::iterator it;
	output_edge << "#number of edge = " << std::dec << edges.size() << std::endl; 
	for (it = edges.begin(); it != edges.end(); it++) {
		Edge *cur_edge = *it;
        //if (cur_edge->rel_type == WEAK_REL)
        //    continue;
		assert(cur_edge);

		uint64_t edge_to = cur_edge->e_to->get_tfl_index();
		if (edge_to == 0) {
			//faked woken
			assert(cur_edge->to);
			assert(cur_edge->to->get_group());
			EventBase *real_event = cur_edge->to->get_group()->get_real_first_event();
			assert(real_event);
			edge_to = real_event->get_tfl_index();
		}
        output_edge << std::dec << cur_edge->e_from->get_tfl_index();
        output_edge << " to "<< std::dec << edge_to;
        output_edge <<"\t" << std::dec << (uint32_t)(log10(cur_edge->e_to->get_abstime() - cur_edge->e_from->get_abstime()));
        output_edge << std::endl;
    }
    output_edge.close();
}

void Graph::tfl_nodes_and_edges(std::string path)
{
    tfl_nodes(path);
    tfl_edges(path);
}

void Graph::show_event_info(uint64_t tfl_index, std::ostream &out)
{
	if (event_lists_ptr == nullptr)
		out << "No tfl infomation maintained in the graph " << std::endl;
	EventBase *event = event_lists_ptr->tfl_index2event(tfl_index);
	if (event == nullptr)
		out << "No corresponding event found" << std::endl;
	event->streamout_event(out);
	out << std::endl;
}

void Graph::streamout_nodes(std::string path) 
{
    std::string node_path(path + "_nodes");
    std::ofstream output(node_path);
    std::vector<Node *>::iterator it;
    
	std::cout << "number of nodes = " << std::dec << nodes.size() << std::endl; 
    for (it = nodes.begin(); it != nodes.end(); it++) {
        Node *node = *it;
        assert(node->get_group());
		output << "#Group " << node->get_gid() << std::endl;
        node->get_group()->streamout_group(output);
    }
    output.close();

}

void Graph::streamout_edges(std::string path)
{
    std::string edge_path(path + "_edges");
    std::ofstream output_edge(edge_path);
	std::vector<Edge *>::iterator it;
	std::cout << "number of edge = " << std::dec << edges.size() << std::endl; 
	int i = 0;
	for (it = edges.begin(); it != edges.end(); it++) {
		Edge *cur_edge = *it;
		assert(cur_edge);
		assert(cur_edge->from);
        output_edge << "\t" << cur_edge->e_from->get_procname();
        output_edge << ' ' << std::hex << cur_edge->from->get_gid() << ' ';
        output_edge << std::fixed << std::setprecision(1) << cur_edge->e_from->get_abstime();
        output_edge << "->";
        output_edge << cur_edge->e_to->get_procname();
        output_edge << ' ' << std::hex << cur_edge->e_to->get_group_id() << ' ';
        output_edge << std::fixed << std::setprecision(1) << cur_edge->e_to->get_abstime() << std::endl;
		++i;
	}
    output_edge.close();
}

void Graph::streamout_nodes_and_edges(std::string path)
{
    streamout_nodes(path);
    streamout_edges(path);
}

void Graph::check_heuristics()
{
    std::vector<Node *>::iterator it;
	std::cout << "number of nodes = " << std::dec << nodes.size() << std::endl; 
    for (it = nodes.begin(); it != nodes.end(); it++) {
        Node *node = *it;
        assert(node->get_group());
        int divide_by_msg = node->get_group()->is_divide_by_msg();

        if (divide_by_msg & DivideOldGroup) {
            Group *peer_group = groups_ptr->get_group_by_gid(node->get_gid() + 1);
            if (peer_group != nullptr) {
                std::cout << node->get_group()->get_procname();
                std::cout << " Node 0x" << std::hex << node->get_gid();
                std::cout << " and Node 0x" << std::hex << peer_group->get_group_id();
                std::cout << " are divided by msg heuristics" << std::endl;

                std::set<std::string> peer_set = peer_group->get_group_peer();
                std::set<std::string> cur_set = node->get_group()->get_group_peer();
                std::set<std::string>::iterator it;

                for (it = cur_set.begin(); it != cur_set.end(); it++)
                    std::cout << "\t" << *it;
                std::cout << std::endl;

                for (it = peer_set.begin(); it != peer_set.end(); it++)
                    std::cout << "\t" << *it;
                std::cout << std::endl;
            }
        }

        if (divide_by_msg & DivideNewGroup) {
            Group *peer_group = groups_ptr->get_group_by_gid(node->get_gid() - 1);
            
            if (peer_group != nullptr) {
                std::cout << node->get_group()->get_procname();
                std::cout << " Node 0x" << std::hex << peer_group->get_group_id();
                std::cout << " and Node 0x" << std::hex << node->get_gid();
                std::cout << " are divided by msg heuristics" << std::endl;

                std::set<std::string> peer_set = peer_group->get_group_peer();
                std::set<std::string> cur_set = node->get_group()->get_group_peer();
                std::set<std::string>::iterator it;

                for (it = peer_set.begin(); it != peer_set.end(); it++)
                    std::cout << "\t" << *it;
                std::cout << std::endl;
                for (it = cur_set.begin(); it != cur_set.end(); it++)
                    std::cout << "\t" << *it;
                std::cout << std::endl;
            }
        }
    }//end of for
}
