#include "critical_path.hpp"

CriticalPath::CriticalPath(Graph *graph_, Node *root_, Node *end_, std::string path_ )
:graph(graph_), root_node(root_), end_node(end_), output_path(path_)
{
    if (root_node != nullptr && end_node != nullptr) {
		bool success = extract_path();
		if (success && output_path != "critical_path.log")
			save_weak_edges_to_file(output_path);

        if (success)
        	save_path_to_file("critical_path.log", detailed_path); 
    }
	//maintain a reachable node set to exclude recursively visit of unreachable node
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
	//for (int i = 0; i < ident; i++) {
		//std::cout << ' ';
	//}
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

    //EventBase *last_update = end_node->get_group()->contains_view_update();
    //assert(last_update);

    detailed_path = critical_path_to(0, end_node, end_node->get_group()->get_last_event());
    //path = critical_path_to(0, end_node, end_node->get_group()->get_last_event()->get_abstime());
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

CriticalPath::edge_map_t CriticalPath::in_edges_before_deadline(event_to_edge_map_t in_edges, double deadline)
{
    edge_map_t ret;
    event_to_edge_map_t::iterator it;

    ret.clear();
    for (it = in_edges.begin(); it != in_edges.end(); it++) {
        if (it->second->from == it->second->to)
            continue;
        if ((it->first)->get_abstime() < deadline + 0.005)
            ret[it->second] = it->first->get_group_id();
    }
    return ret;
}

std::vector<Edge *> CriticalPath::sort_in_edges(edge_map_t in_edges)
{
    edge_map_t::iterator it;
    std::vector<Edge *> sorted_edges;

    for (it = in_edges.begin(); it != in_edges.end(); it++) {
        sorted_edges.push_back(it->first);
        sort(sorted_edges.begin(), sorted_edges.end(), CriticalPath::edge_weight_compare);
    }
	return sorted_edges;
}

std::vector<Node *> CriticalPath::sort_nodes_from_edges(edge_map_t in_edges)
{
    edge_map_t::iterator it;
    std::vector<Edge *> sorted_edges;

    for (it = in_edges.begin(); it != in_edges.end(); it++) {
        sorted_edges.push_back(it->first);
        sort(sorted_edges.begin(), sorted_edges.end(), CriticalPath::edge_weight_compare);
    }

    std::vector<Edge *>::iterator edge_it;
    std::map<Node *, bool> visited_maps;
    std::vector<Node *> candidates;

    for (edge_it = sorted_edges.begin(); edge_it != sorted_edges.end(); edge_it++) {
        if (visited_maps.find((*edge_it)->from) == visited_maps.end())
            candidates.push_back((*edge_it)->from);
        visited_maps[(*edge_it)->from] = true;
    }
#ifdef NODEWEIGHT
    sort(candidates.begin(), candidates.end(), CriticalPath::node_weight_compare);
#endif
    return candidates;   
}

std::vector<Element *> CriticalPath::add_weak_edge(int ident, Node *cur_node, EventBase *deadline)
{
    event_to_edge_map_t weak_edges = cur_node->get_in_weak_edges();
    std::vector<Element *> ret_path;
    ret_path.clear();

    if (weak_edges.size() == 0) {
		print_ident(ident);
		Node *prev = graph->id_to_node(cur_node->get_gid() -1);
		if (prev != nullptr) {
			ret_path = critical_path_to(ident + 1, prev, prev->get_group()->get_last_event());
			if (ret_path.size() > 0) {
				Element *cur_element =  new Element(cur_node->get_group()->get_first_event(), cur_node, 
						cur_node->get_group()->get_last_event());
				ret_path.push_back(cur_element);
				store_weak_edges[cur_node->get_gid()] = prev->get_gid();
			}
		}
        return ret_path;
    }

    event_to_edge_map_t::iterator it = weak_edges.begin();
    Node *prev_weak = it->second->from;

	print_ident(ident);
    std::cout << "\tWeak edge from 0x";
    if (prev_weak == nullptr) {
        std::cout << std::hex << weak_edges[0]->e_from->get_group_id() \
            << "to 0x" << std::hex << cur_node->get_tid() << " is not in graph" << std::endl;
        return ret_path;
    }

    std::cout << std::hex << prev_weak->get_gid();
    std::cout << " to 0x" << std::hex << cur_node->get_gid() << std::endl;
    ret_path = critical_path_to(ident + 1, prev_weak, prev_weak->get_group()->get_last_event());
	if (ret_path.size() > 0) {
		Element *cur_element =  new Element(cur_node->get_group()->get_first_event(), cur_node, 
			cur_node->get_group()->get_last_event());
		ret_path.push_back(cur_element);
		store_weak_edges[cur_node->get_gid()] = prev_weak->get_gid();
	}
	return ret_path;
}

std::vector<Node *> CriticalPath::add_weak_edge(int ident, Node *cur_node, double deadline)
{
    event_to_edge_map_t weak_edges = cur_node->get_in_weak_edges();
    std::vector<Node *> ret_path;
    ret_path.clear();

    if (weak_edges.size() == 0) {
		print_ident(ident);
        std::cout << "\tNo weak edge and stop searching" << std::endl; 
		Node *prev = graph->id_to_node(cur_node->get_gid() -1);
		if (prev != nullptr)
			return critical_path_to(ident + 1, prev, deadline);
        return ret_path;
    }

    event_to_edge_map_t::iterator it = weak_edges.begin();
    Node *prev_weak = it->second->from;

	print_ident(ident);
    std::cout << "\tWeak edge from 0x";
    if (prev_weak == nullptr) {
        std::cout << std::hex << weak_edges[0]->e_from->get_group_id() \
            << "to 0x" << std::hex << cur_node->get_tid() << " is not in graph\n";
        return ret_path;
    }

    //Node *prev_weak = end->prev_weak_in_thread();

    std::cout << std::hex << prev_weak->get_gid();
    std::cout << " to 0x" << std::hex << cur_node->get_gid() << std::endl;
    return critical_path_to(ident + 1, prev_weak, deadline);
}

std::vector<Element *> CriticalPath::critical_path_to(int ident, Node *cur_node, EventBase *deadline)
{
	std::vector<Element *> ret_path;
	ret_path.clear();
    if (cur_node == nullptr) {
		print_ident(ident);
        std::cout << "Path reaches nullptr" << std::endl;
        return ret_path;
    }
    
    if (cur_node->get_group()->get_last_event()->get_abstime()
        < root_node->get_group()->get_first_event()->get_abstime()) {
		print_ident(ident);
        std::cout << "Path goes over root node" << std::endl;
        return ret_path;
    }

    if (cur_node == root_node) {
		Element *cur_element =  new Element(cur_node->get_group()->get_first_event(), cur_node, 
										cur_node->get_group()->get_last_event());
        ret_path.push_back(cur_element);
		print_ident(ident);
        std::cout << "Path reaches root node" << std::endl;
        return ret_path;
    }

    edge_map_t in_edges = in_edges_before_deadline(cur_node->get_in_edges(), deadline->get_abstime());
    if (in_edges.size() == 0) {
        //weak edge
        ret_path = add_weak_edge(ident + 1, cur_node, deadline);
        if (ret_path.size() > 0) {
			Element *cur_element = new Element(cur_node->get_group()->get_first_event(), cur_node, deadline);
            ret_path.push_back(cur_element);
		}
        return ret_path;
    }

	print_ident(ident);
    std::cout << "Number of edges to 0x" << std::hex << cur_node->get_gid()\
            << " = " << in_edges.size() << std::endl;
    
    std::vector<Edge *> candidates = sort_in_edges(in_edges);
	for (auto edge_it = candidates.begin(); edge_it != candidates.end(); edge_it++) {
		Node *from = (*edge_it)->from;
        assert(from != nullptr);
		print_ident(ident);
		std::cout << "Select Edge from 0x" << std::hex << from->get_gid() << " to 0x" << cur_node->get_gid() << std::endl;
        ret_path = critical_path_to(ident + 1, from, (*edge_it)->e_from);

		if (ret_path.size() > 0) {
			Element *cur_element = new Element((*edge_it)->e_to, cur_node, deadline);
			ret_path.push_back(cur_element);
			return ret_path;
		}	
	}
    return ret_path;
}

std::vector<Node *> CriticalPath::critical_path_to(int ident, Node *cur_node, double deadline)
{
    std::vector<Node *> ret_path;
    ret_path.clear();
    if (cur_node == nullptr) {
		print_ident(ident);
        std::cout << "Path reaches nullptr" << std::endl;
        return ret_path;
    }
    
    if (cur_node->get_group()->get_last_event()->get_abstime()
        < root_node->get_group()->get_first_event()->get_abstime()) {
		print_ident(ident);
        std::cout << "Path goes over root node" << std::endl;
        return ret_path;
    }

    if (cur_node == root_node) {
        ret_path.push_back(cur_node);
		print_ident(ident);
        std::cout << "Path reaches root node" << std::endl;
        return ret_path;
    }

    edge_map_t in_edges = in_edges_before_deadline(cur_node->get_in_edges(), deadline);
    if (in_edges.size() == 0) {
        //process weak edge cases
        ret_path = add_weak_edge(ident + 1, cur_node, deadline);
        if (ret_path.size() > 0)
            ret_path.push_back(cur_node);
        return ret_path;
    }

	print_ident(ident);
    std::cout << "Number of edges to 0x" << std::hex << cur_node->get_gid()\
            << " = " << in_edges.size() << std::endl;
    
    std::vector<Node *> candidates = sort_nodes_from_edges(in_edges);
    std::vector<Node *>::iterator node_it;
    for (node_it = candidates.begin(); node_it != candidates.end(); node_it++) {
        Node *from = *node_it;
        assert(from != nullptr);
		print_ident(ident);
		std::cout << "Select Edge from 0x" << std::hex << from->get_gid() << " to 0x" << cur_node->get_gid() << std::endl;
        ret_path = critical_path_to(ident + 1, from, calculate_deadline(in_edges, from));

        if (ret_path.size() > 0) {
           ret_path.push_back(cur_node);
           return ret_path;
        }
    }
    return ret_path;
}

double CriticalPath::calculate_deadline(edge_map_t in_edges, Node *from)
{
    edge_map_t::iterator it;
    for (it = in_edges.begin(); it != in_edges.end(); it++) {
       if (it->first->from == from)
            return it->first->e_from->get_abstime();
    }
    //should not come here
    assert(false);
    return 0;
}

void CriticalPath::save_path(std::ofstream &output)
{
	output << "\tPath from 0x" << std::hex << root_node->get_gid() \
			<< " to 0x" << std::hex << end_node->get_gid() << std::endl;
    output << "\tCritical path size = " << detailed_path.size() << std::endl;

	for (auto element : detailed_path) {
        Group *cur_group = element->segment->get_group();
        output << std::hex << " \t-> 0x" << cur_group->get_group_id() << "\t" << cur_group->get_procname();
        output << "\t" << std::fixed << std::setprecision(1) << cur_group->calculate_time_span() << std::endl;
        cur_group->streamout_group(output, element->in, element->out);
    }
    output << std::endl;
}

void CriticalPath::save_path_to_file(std::string filepath)
{
	//write data to log
    std::ofstream output(filepath);
    std::vector<Node *>::iterator pit;
    output << "Critical path size = " << path.size() << std::endl;
    for (pit = path.begin(); pit != path.end(); pit++) {
        Group *cur_group = (*pit)->get_group();
        output << std::hex << " \t-> 0x" << cur_group->get_group_id() << "\t" << cur_group->get_procname();
        output << "\t" << std::fixed << std::setprecision(1) << cur_group->calculate_time_span() << std::endl;
        cur_group->streamout_group(output);
    }
    output << std::endl;
    output.close();
}

void CriticalPath::save_path_to_file(std::string filepath, std::vector<Node *> &path)
{
	//write data to log
	std::ofstream output(filepath, std::ios::app);
	
    std::vector<Node *>::iterator pit;
    output << "Critical path size = " << path.size() << std::endl;
    for (pit = path.begin(); pit != path.end(); pit++) {
        Group *cur_group = (*pit)->get_group();
        output << std::hex << " \t-> 0x" << cur_group->get_group_id();
        output << "\t" << cur_group->get_procname();
        output << "\t" << std::fixed << std::setprecision(1);
        output << cur_group->calculate_time_span() << std::endl;
    }
    output << std::endl;
    output.close();
}

void CriticalPath::save_path_to_file(std::string filepath, std::vector<Element *> &path)
{
	//write data to log
	std::ofstream output(filepath, std::ios::app);

	output << "Path from 0x" << std::hex << root_node->get_gid() \
			<< " to 0x" << std::hex << end_node->get_gid() << std::endl;
    output << "Critical path size = " << path.size() << std::endl;

	for (auto element : path) {
        Group *cur_group = element->segment->get_group();
        output << std::hex << " \t-> 0x" << cur_group->get_group_id() << "\t" << cur_group->get_procname();
        output << "\t" << std::fixed << std::setprecision(1) << cur_group->calculate_time_span() << std::endl;
        cur_group->streamout_group(output, element->in, element->out);
    }

    output << std::endl;
    output.close();
}

void CriticalPath::save_weak_edges_to_file(std::string output_path)
{
	std::ofstream output(output_path, std::ios::app);
	for (auto element : store_weak_edges) {
		Node *to_node = graph->id_to_node(element.first);
		assert(to_node);
		Node *from_node = graph->id_to_node(element.second);
		assert(from_node);
	
		Group *to = to_node->get_group();
		assert(to);
		Group *from = from_node->get_group();
		assert(from);
		//if (to->get_group_id() == from->get_group_id() - 1) {
		output << std::dec <<  from->get_last_event()->get_tfl_index();
		output  << "\t" << std::dec << to->get_real_first_event()->get_tfl_index();
		output << std::endl;
		
	}
	output.close();
}
