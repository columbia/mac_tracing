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
		assert(e->e_to->get_group_id() == get_gid());
		in_edges.insert(std::pair<EventBase *, Edge*> (e->e_to, e));
		e->set_edge_to_node(this);
    } else {
		assert(exist_edge->e_to == e->e_to);
		assert(exist_edge->e_to->get_group_id() == get_gid());
		in_edges.insert(std::pair<EventBase *, Edge*> (e->e_to, exist_edge));
		exist_edge->set_edge_to_node(this);
    }
	inc_in();
    return ret;
}

bool Node::add_in_weak_edge(Edge *e)
{
	assert(e);
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
		assert(e);
        assert(e->e_to);
		std::pair<EventBase *, Edge *> item(e->e_to, e);
		in_weak_edges.insert(item);
		e->set_edge_to_node(this);
    } else {
		assert(exist_edge);
		assert(exist_edge->e_to);
		std::pair<EventBase *, Edge*> item(exist_edge->e_to, exist_edge);
		in_weak_edges.insert(item);
		exist_edge->set_edge_to_node(this);
    }
	inc_weak_in();
    return ret;
}

bool Node::add_out_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
		std::pair<EventBase *, Edge*> item(e->e_from, e);
		out_edges.insert(item);
		e->set_edge_from_node(this);
    } else {
		std::pair<EventBase *, Edge*> item(e->e_to, exist_edge);
		out_edges.insert(item);
		exist_edge->set_edge_from_node(this);
    }
	inc_out();
    return ret;
}

bool Node::add_out_weak_edge(Edge *e)
{
    Edge *exist_edge = parent->check_and_add_edge(e);
    bool ret = exist_edge == nullptr ? true : false;
    if (exist_edge == nullptr) {
		out_weak_edges.insert(std::pair<EventBase *, Edge*> (e->e_from, e));
        e->set_edge_from_node(this);
    } else {
		out_weak_edges.insert(std::pair<EventBase *, Edge*> (e->e_from, exist_edge));
        exist_edge->set_edge_from_node(this);
    }
	inc_weak_out();
    return ret;
}

Node::edge_map_t &Node::get_in_edges()
{
    return in_edges;
}

Node::edge_map_t &Node::get_out_edges()
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
	auto map = out_edge? out_edges: in_edges;
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

    if (find_edge_in_map(edge, out_edge) == false)
        return;

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
	assert(last_event);
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

EventBase *Node::index_to_event(int index)
{
    std::list<EventBase *> &container = group->get_container();
    std::list<EventBase *>::iterator it = container.begin();

    if (index >= container.size() || index < 0)
        return nullptr;

    std::advance(it, index);
    return *it;    
}

void Node::show_event_detail(int index, std::ostream &out)
{
	EventBase *ev = index_to_event(index);
	if (ev)
		ev->streamout_event(out);
	else
		out << "Invalid index in current node 0x" << get_gid() << std::endl;
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
    construct_inside = true;

    groups_ptr = new Groups(event_lists_ptr);
    create_graph = true;

	tfl_lists = nullptr;

    event_lists_ptr->streamout_all_event(event_output_path);
    groups_ptr->streamout_groups(group_output_path);

	edges.clear();
	nodes.clear();
    nodes_map.clear();
    edge_map.clear();
}

Graph::Graph(Groups *_groups_ptr)
{
    event_lists_ptr = nullptr;
	tfl_lists = nullptr;
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
		for (auto node : nodes) {
            assert(node);
            delete(node);
        }

		for (auto edge : edges) {
            assert(edge);
            delete(edge);
        }
    }

    if (construct_inside) {
        assert(groups_ptr && event_lists_ptr);
        delete groups_ptr;
        delete event_lists_ptr;
    }

	if (tfl_lists)
		delete tfl_lists;

    nodes.clear();
    edges.clear();
}

// thread unsafe
void Graph::remove_node(Node *node)
{
    assert(node != nullptr);
    node->remove_edges();

	auto it = find(nodes.begin(), nodes.end(), node);
    assert(it != nodes.end() && *it == node);
    nodes.erase(it);

	auto it_1 = nodes_map.find(node->get_group());
	assert(it_1 != nodes_map.end() && it_1->second == node);
	nodes_map.erase(it_1);
	assert(find(nodes.begin(), nodes.end(), node) == nodes.end());
    delete node;
}

void Graph::remove_edge(Edge *edge)
{
    assert(edge != nullptr);
	auto it = find(edges.begin(), edges.end(), edge);
    if (it != edges.end()) {
        edges.erase(it);
		assert(find(edges.begin(), edges.end(), edge) == edges.end());
        delete edge;
    }
	//TODO: also remove from the edge map with lock
}

Node *Graph::check_and_add_node(Node *node)
{
    Node *ret = nullptr;
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
	auto range = edge_map.equal_range(e->e_from);
	for (auto it = range.first; it != range.second; it++) {
		if (*(it->second) == *e) {
			ret = it->second;
			graph_edge_mtx.unlock();
			return ret;
		}
	}
    //if no the same begin and end, add new edge
    edge_map.emplace(e->e_from, e);
    edges.push_back(e);
    graph_edge_mtx.unlock();
    return ret;
} 

Node *Graph::id_to_node(group_id_t gid)
{
    Group *id_to_group = groups_ptr->get_group_by_gid(gid);

    if (id_to_group == nullptr) {
        LOG_S(INFO) << "Unknown group id 0x" << std::hex << gid << std::endl;
        return nullptr;
    }

    if (nodes_map.find(id_to_group) != nodes_map.end())
        return nodes_map[id_to_group];
    
    LOG_S(INFO)  << " Node 0x" << std::hex << gid << " not included in graph" << std::endl;
    return nullptr;
}

EventBase *Graph::get_event(group_id_t gid, int index)
{
	Node *node = id_to_node(gid);
	if (!node)
		return nullptr;
	return node->index_to_event(index);
}

std::vector<Node *> Graph::get_nodes_for_tid(tid_t tid)
{
    std::vector<Node *> result;
	result.clear();

	for (auto element : groups_ptr->get_groups_by_tid(tid)) {
        assert(nodes_map.find(element.second) != nodes_map.end());
        result.push_back(nodes_map[element.second]);
    }
    return result;
}

std::vector<Node *> Graph::nodes_between(Node *cur_node, double timestamp)
{
    std::vector<Node *> ret = get_nodes_for_tid(cur_node->get_tid());
	auto it = find(ret.begin(), ret.end(), cur_node);
    ret.erase(ret.begin(), it);

    for (it = ret.begin(); it != ret.end(); it++)
		if ((*it)->get_begin_time() > timestamp)
            break;
    ret.erase(it, ret.end());

    return ret;
}

Node *Graph::node_of(EventBase *event)
{
	Group *group = groups_ptr->group_of(event);
	if (group == nullptr)
		return nullptr;
	if (nodes_map.find(group) != nodes_map.end())
		return nodes_map[group];
	return nullptr;
}

Group *Graph::group_of(EventBase *event)
{
	if (groups_ptr == nullptr)
		return nullptr;
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
    LOG_S(INFO) << "Graph size: " << std::endl;
    LOG_S(INFO) << "Edges numer = " << std::dec << edges.size() << std::endl;
    LOG_S(INFO) << "Nodes numer = " << std::dec << nodes.size() << std::endl;

    std::map<uint32_t, uint32_t> edge_categories;
    std::set<std::string> procs_set;
    std::vector<uint64_t> multin_grs; 

    edge_categories.clear();
    procs_set.clear();
    multin_grs.clear();
	
	for (auto cur_edge : edges)
        edge_categories[cur_edge->rel_type] = edge_categories[cur_edge->rel_type] + 1;

	for (auto element : edge_categories)
        LOG_S(INFO) << "Edge Category[" << element.first << "] = " << std::dec << element.second << std::endl;

	for (auto node : nodes) {
        assert(node->get_group());
        if (node->get_in() > 1)
            multin_grs.push_back(node->get_gid());
        procs_set.insert(node->get_procname());
    }

	LOG_S(INFO) << "number of multiple-incoming-edge nodes = " << std::dec << multin_grs.size() << std::endl; 
//	for (auto multin_gr : multin_grs)
//        std::cout << "\t0x" << std::hex << multin_gr;
//    std::cout << std::endl;

	LOG_S(INFO) << "number of procs = " << std::dec <<  procs_set.size() << std::endl; 
	for (auto proc : procs_set) 
        LOG_S(INFO) << std::left << std::setw(32) << proc;
    LOG_S(INFO) << std::endl;
}

void Graph::direct_communicate_procs(std::string procname, std::ostream &out)
{
    std::set<std::string> procs_set;
	for (auto cur_edge : edges) {
		if (cur_edge->e_from->get_procname() == procname)
			procs_set.insert(cur_edge->e_to->get_procname());
		else if(cur_edge->e_to->get_procname() == procname)
			procs_set.insert(cur_edge->e_from->get_procname());
	}

	out << "Interaction to proc " << procname << ":\n";
	for (auto proc : procs_set)
		out << "\t" << proc << std::endl;
}

void Graph::streamout_nodes(std::string path) 
{
    std::string node_path(path + "_nodes");
    std::ofstream output(node_path);
    
	LOG_S(INFO) << "number of nodes = " << std::dec << nodes.size() << std::endl; 
	for (auto node : nodes) {
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

	LOG_S(INFO) << "number of edge = " << std::dec << edges.size() << std::endl; 

	int i = 0;
	for (auto cur_edge : edges) {
		assert(cur_edge);
		assert(cur_edge->from);
        output_edge << "\t" << cur_edge->e_from->get_procname()\
        	<< ' ' << std::hex << cur_edge->from->get_gid()\
			<< ' ' << std::fixed << std::setprecision(1) << cur_edge->e_from->get_abstime()\
        	<< " -> "\
        	<< cur_edge->e_to->get_procname()\
        	<< ' ' << std::hex << cur_edge->e_to->get_group_id()\
			<< ' ' << std::fixed << std::setprecision(1) << cur_edge->e_to->get_abstime()\
			<< std::endl;
		++i;
	}
    output_edge.close();
}

void Graph::streamout_nodes_and_edges(std::string path)
{
    streamout_nodes(path);
    streamout_edges(path);
}

void Graph::generate_tfl_list()
{
    std::list<EventBase *> event_list;
	for (auto cur_node : nodes) {
        Group *cur_group = cur_node->get_group();
        assert(cur_group);
        std::list<EventBase *> &container = cur_group->get_container();
        event_list.insert(event_list.end(), container.begin(), container.end());
    }
    event_list.sort(Parse::EventComparator::compare_time);

	tfl_lists = new EventLists(event_list);
	tfl_lists->label_tfl_index(event_list);
}

void Graph::tfl_by_thread(std::string path) 
{
	if (!tfl_lists)
		generate_tfl_list();

	tfl_lists->tfl_by_thread(path);
}

void Graph::tfl_nodes(std::string path)
{
    std::string event_path(path + "_events");
	
	if (!tfl_lists)
		generate_tfl_list();

    tfl_lists->tfl_all_event(event_path);

}

void Graph::tfl_edges(std::string path)
{
    std::string edge_path(path + "_edges");
    std::ofstream output_edge(edge_path);
	if (!tfl_lists)
		generate_tfl_list();

	output_edge << "#number of edge = " << std::dec << edges.size() << std::endl; 

	for (auto cur_edge : edges) {
		assert(cur_edge);
		if (cur_edge->to == nullptr) {
			std::cout << "node in edge at " << std::fixed << std::setprecision(1)\
				<< cur_edge->e_to->get_abstime()\
				<< " is not initialized " << std::endl;  
			continue;
		}

		uint64_t edge_to = cur_edge->e_to->get_tfl_index();

		if (edge_to == 0) {
			//faked woken
			assert(cur_edge->to);
			assert(cur_edge->to->get_group());
			EventBase *real_event = cur_edge->to->get_group()->get_real_first_event();
			assert(real_event);
			edge_to = real_event->get_tfl_index();
		}

		uint32_t edge_time_delta = (uint32_t)(log10(cur_edge->e_to->get_abstime() - cur_edge->e_from->get_abstime()));

        output_edge << std::dec << cur_edge->e_from->get_tfl_index()\
        		<< " to "<< std::dec << edge_to\
        		<< "\t" << std::dec << edge_time_delta\
        		<< std::endl;
    }

    output_edge.close();
}

void Graph::tfl_edge_distances(std::string path)
{
	if (!tfl_lists)
		generate_tfl_list();
    std::string distance_path(path + "_distance");
	tfl_lists->tfl_peer_distance(distance_path);
}

void Graph::tfl_nodes_and_edges(std::string path)
{
	if (!tfl_lists)
		generate_tfl_list();
    tfl_nodes(path);
    tfl_edges(path);
	tfl_edge_distances(path);
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

/////////////////////////////////////////////////////////////////////////////////////////////
void Graph::check_heuristics()
{
	std::cout << "number of nodes = " << std::dec << nodes.size() << std::endl; 

	for (auto node : nodes) {
        assert(node->get_group());
        Group *peer_group = groups_ptr->get_group_by_gid(node->get_gid() - 1);
		if (!peer_group)
			continue;

        int divide_by_msg = node->get_group()->is_divide_by_msg();

		if (divide_by_msg & DivideOldGroup) {
			std::cout << node->get_group()->get_procname()\
				<< " Node 0x" << std::hex << node->get_gid()\
				<< " and "\
				<< " Node 0x" << std::hex << peer_group->get_group_id()\
				<< " are divided by msg heuristics"\
				<< std::endl;

			for (auto proc : node->get_group()->get_group_peer())
				std::cout << "\t" << proc;
			std::cout << std::endl;

			for (auto proc : peer_group->get_group_peer())
				std::cout << "\t" << proc;
			std::cout << std::endl;
		}

		if (divide_by_msg & DivideNewGroup) {

			std::cout << node->get_group()->get_procname()\
				<< " Node 0x" << std::hex << peer_group->get_group_id()\
				<< " and "\
				<< " Node 0x" << std::hex << node->get_gid()\
				<< " are divided by msg heuristics"\
				<< std::endl;

			for (auto proc : peer_group->get_group_peer())
				std::cout << "\t" << proc;
			std::cout << std::endl;

			for (auto proc : node->get_group()->get_group_peer())
				std::cout << "\t" << proc;
			std::cout << std::endl;

		}
	}//end of for
}
