#include "critical_path.hpp"
#define DEBUG_CPATH 0

CriticalPath::CriticalPath(Graph *graph_, Node *root_, Node *end_, std::string path)
:graph(graph_), root_node(root_), end_node(end_)
{
	visited.clear();
	if (!root_node || !end_node) {
		LOG_S(INFO) << "Unable to initiate object" << std::endl;
		return;
	}
	root_ev = root_node->get_group()->get_first_event();
	end_ev = end_node->get_group()->get_last_event();
	reachable = extract_path();
	if (reachable)
		append_path_to_file(path, detailed_path); 
}

CriticalPath::CriticalPath(Graph *graph_, uint64_t root_id, uint64_t end_id, std::string path_)
:graph(graph_)
{
	root_node = graph->id_to_node(root_id);
	end_node = graph->id_to_node(end_id);
	CriticalPath(graph, root_node, end_node, path_);
}

CriticalPath::CriticalPath(Graph *graph_, Node *root_, Node *end_, EventBase *root_ev_, EventBase *end_ev_, std::string path)
:graph(graph_), root_node(root_), end_node(end_), root_ev(root_ev_), end_ev(end_ev_)
{
	visited.clear();
	if (!root_node || !end_node) {
		LOG_S(INFO) << "Unable to initiate object" << std::endl;
		return;
	}
	reachable = extract_path();
	if (reachable)
		append_path_to_file(path, detailed_path); 
}

CriticalPath::~CriticalPath()
{
    graph = nullptr;
    root_node = nullptr;
    end_node = nullptr;
	for (auto element : detailed_path)
		delete element;
	detailed_path.clear();
}

void CriticalPath::print_ident(int ident)
{
	std::cout << std::hex << std::left << std::setw(3) << ident;
}

bool CriticalPath::extract_path()
{
    assert(root_node && end_node);
    if (end_node == root_node) {
        std::cout << "Root and end are in the same node 0x"\
            << std::hex << root_node->get_gid() << std::endl;   
        return true;
    }

    std::cout << "Begin extracting critical path from id 0x"\
         << std::hex << root_node->get_gid() << " to 0x"\
         << std::hex << end_node->get_gid() << std::endl;
    detailed_path = critical_path_to_root(0, end_node, nullptr);//end_node->get_group()->get_last_event());
	if (detailed_path.size() > 0)
		visited[end_node] = true;
	else
		visited[end_node] = false;

	std::cout << "Finish extracing critical path from id 0x"\
         << std::hex << root_node->get_gid() << " to 0x"\
         << std::hex << end_node->get_gid() << ", length = "\
		 << std::dec << detailed_path.size() << std::endl;
	
	return detailed_path.size() > 0;
}

bool CriticalPath::node_weight_compare(Node *elem1, Node *elem2)
{
    return elem1->time_span() > elem2->time_span();
}

bool CriticalPath::edge_weight_compare(Edge *edge1, Edge *edge2)
{
    if (edge1->rel_type == WEAK_REL && edge2->rel_type != WEAK_REL)
        return false;
    return edge1->get_weight() < edge2->get_weight();
}

//in_edges map: sink event -> edge
CriticalPath::edge_sink_map_t CriticalPath::filt_with_deadline(edge_map_t in_edges, double deadline)
{
    edge_sink_map_t ret;
    ret.clear();
    for (auto it = in_edges.begin(); it != in_edges.end(); it++) {
        if (it->second->from == it->second->to)
            continue;
        if ((it->first)->get_abstime() < deadline + 0.005)
            ret[it->second] = it->first->get_group_id();
    }
    return ret;
}

std::vector<Edge *> CriticalPath::sort_edges(edge_sink_map_t in_edges)
{
    std::vector<Edge *> sorted_edges;

    for (auto it = in_edges.begin(); it != in_edges.end(); it++) {
        sorted_edges.push_back(it->first);
        sort(sorted_edges.begin(), sorted_edges.end(), CriticalPath::edge_weight_compare);
    }
	return sorted_edges;
}

std::vector<Edge *> CriticalPath::filter_with_deadline(edge_map_t in_edges, double deadline)
{
	std::vector<Edge *> ret;
	ret.clear();

	std::unordered_map<Node *, Edge *> visited;
	visited.clear();

	for (auto elem : in_edges) {
		if (elem.second->from  == elem.second->to)
			continue;
		assert(elem.first == elem.second->e_to);

		if (elem.first->get_abstime() < deadline + 0.005
			&& elem.second->e_from->get_abstime() > root_node->get_begin_time()) {
			Node *sel = elem.second->from;
			if (visited.find(sel) != visited.end()) {
				if (visited[sel]->e_from->get_abstime() < elem.second->e_from->get_abstime())
					visited[sel] = elem.second;
			} else {
				visited[sel] = elem.second;
			}
			//ret.push_back(elem.second);
		}
	}
	for (auto elem: visited) {
		ret.push_back(elem.second);
	}
	//sort(ret.begin(), ret.end(), CriticalPath::edge_weight_compare);
	return ret;
}

std::vector<Node *> CriticalPath::get_sorted_nodes(edge_sink_map_t in_edges)
{
    std::vector<Edge *> sorted_edges;
    for (auto it = in_edges.begin(); it != in_edges.end(); it++) {
        sorted_edges.push_back(it->first);
    }
    sort(sorted_edges.begin(), sorted_edges.end(), CriticalPath::edge_weight_compare);

    std::map<Node *, bool> visited_maps;
    std::vector<Node *> candidates;
    for (auto edge_it = sorted_edges.begin(); edge_it != sorted_edges.end(); edge_it++) {
        if (visited_maps.find((*edge_it)->from) == visited_maps.end())
            candidates.push_back((*edge_it)->from);
        visited_maps[(*edge_it)->from] = true;
    }
#ifdef NODEWEIGHT
    sort(candidates.begin(), candidates.end(), CriticalPath::node_weight_compare);
#endif
    return candidates;   
}

bool CriticalPath::valid(int ident, Node *cur_node, Edge *edge)
{
    if (edge != nullptr) {
        assert(edge->from == cur_node);
     }

    if (cur_node == nullptr) {
#if DEBUG_CPATH
		print_ident(ident);
        std::cout << "Path reaches nullptr" << std::endl;
#endif
        return false;
    }
    
	if (!cur_node->get_group()) {
#if DEBUG_CPATH
		std::cout << "Invalid Node 0x" << std::hex << cur_node->get_gid() << std::endl;
#endif
        return false;
	}

	EventBase *last_event = edge != nullptr ? edge->e_from : end_ev;

    if (last_event->get_abstime() < root_ev->get_abstime()) {
#if DEBUG_CPATH
		print_ident(ident);
        std::cout << "Path goes over root node" << std::endl;
#endif
        return false;
    }

    if (cur_node == root_node) {
		print_ident(ident);
        std::cout << "Path reaches root node" << std::endl;
    }
    return true;
}

std::vector<Element *> CriticalPath::critical_path_to_root(int ident, Node *cur_node, Edge *edge) 
{
	std::vector<Element *> ret_path;
	ret_path.clear();
	if (!cur_node)
		return ret_path;

	if (visited.find(cur_node) != visited.end()) {
		if (visited[cur_node] == false)
			return ret_path;
	}

    if (!valid(ident, cur_node, edge)) {
		LOG_S(INFO) << std::dec << ident <<  " " << std::hex << cur_node->get_gid() << " returned " << std::endl;
        return ret_path;
	}
#if DEBUG_CPATH
	LOG_S(INFO) << std::dec << ident << " search node 0x"  << std::hex << cur_node->get_gid() << std::endl;
#endif

	Group *cur_group = cur_node->get_group();
    EventBase *end_event = edge == nullptr? end_ev : edge->e_from;

    if (cur_node == root_node) {
		Element *cur_element = new Element(nullptr,
                                        cur_group->get_first_event(),
										cur_node, 
                                        end_event,
                                        false);
        ret_path.push_back(cur_element);
		visited[cur_node]= true;
        return ret_path;
    }

    //try strong edges
    auto candidates = filter_with_deadline(cur_node->get_in_edges(), end_event->get_abstime());
#if DEBUG_CPATH
	LOG_S(INFO) << std::dec << ident << " Incomming strong edges  = " << candidates.size() << std::endl;  
#endif
    //std::vector<Edge *> candidates = sort_edges(in_edges);
    for (auto s_edge: candidates) {
		Node *from = s_edge->from;
		assert(s_edge->to == cur_node);
        assert(from != nullptr);
        ret_path = critical_path_to_root(ident + 1, s_edge->from, s_edge);
		if (ret_path.size() > 0) {
#if DEBUG_CPATH
            print_ident(ident);
            std::cout << "Strong Edge from 0x"\
                << std::hex << from->get_gid()\
                << " to 0x"\
                << cur_node->get_gid()\
                << std::endl;
#endif
			Element *cur_element = new Element(s_edge, s_edge->e_to, cur_node, end_event, false);
			ret_path.push_back(cur_element);
			visited[cur_node]= true;
			return ret_path;
		}	
	}

    //try weak edges
    //auto weak_edges = filt_with_deadline(cur_node->get_in_weak_edges(), end_event->get_abstime());
    //std::vector<Edge *> weak_candidates = sort_edges(weak_edges);
	auto weak_candidates = filter_with_deadline(cur_node->get_in_weak_edges(), end_event->get_abstime());
#if DEBUG_CPATH
	LOG_S(INFO) << std::dec << ident << " Incomming weak edges  = " << weak_candidates.size() << std::endl;  
#endif
    for (auto w_edge: weak_candidates) {
        Node *from = w_edge->from;
        assert(from != nullptr);
        ret_path = critical_path_to_root(ident + 1, from, w_edge);

        if (ret_path.size() > 0) {//element are reached with a weak edge
#if DEBUG_CPATH
			print_ident(ident);
			std::cout << "Weak edge from 0x"\
				<< std::hex << from->get_gid()\
				<< " to 0x"\
				<< cur_node->get_gid()\
				<< std::endl;
#endif
            Element *cur_element = new Element(w_edge, w_edge->e_to, cur_node, end_event, true);
            ret_path.push_back(cur_element);
			visited[cur_node]= true;
            return ret_path;
        }
    }
	assert(ret_path.size() == 0);
	visited[cur_node] = false;
    return ret_path;
}

void CriticalPath::append_path_to_file(std::string filepath, std::vector<Element *> &path)
{
	//write data to log
	std::ofstream output(filepath, std::ios::app);

	output << "Path from 0x" << std::hex << root_node->get_gid() \
			<< " to 0x" << std::hex << end_node->get_gid() << std::endl;
    output << "Critical path size = " << path.size() << std::endl;

	for (auto element : path) {
        Group *cur_group = element->node()->get_group();
        output << std::hex << " \t-> 0x" << cur_group->get_group_id()\
               << "\t" << cur_group->get_procname()\
               << "\t" << std::fixed << std::setprecision(1)\
               << cur_group->calculate_time_span() << std::endl;
        cur_group->streamout_group(output, element->begin(), element->end());
    }
    output << std::endl;
    output.close();
}
