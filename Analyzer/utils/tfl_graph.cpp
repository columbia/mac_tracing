#include "tfl_graph.hpp"
#include "critical_path.hpp"

std::map<Edge *, bool> &TraceForLearn::collect_inspect_edges()
{
	for (auto edge: edges) {
		//discard edges with events over 4096 events
		if (edge->from->get_size() > 4096
			|| edge->to->get_size() > 4096)
			continue;

		switch(edge->rel_type) {
			case MSGP_NEXT_REL:
				inspect_edges[edge] = true;
				break;
			case WEAK_REL:
				inspect_edges[edge] = false;
				break;
			default:
				break;
		}
	}

	divide_edges();
	return inspect_edges;
}

void TraceForLearn::print_edge(Edge *edge, std::ofstream &out)
{
	out << "\tEdge " << std::dec << edge->rel_type << std::endl;

	EventBase *from_event = edge->e_from;
	out << "\t\tfrom event" << std::endl; 
	out << "\t\t";
	from_event->streamout_event(out);
	out << std::endl;

	EventBase *to_event = edge->e_to;
	out << "\t\tto event" << std::endl;
	out << "\t\t";
	to_event->streamout_event(out);
	out << std::endl;

}

bool TraceForLearn::similar(Edge *edge1, Edge *edge2)
{
	return (edge1->rel_type == edge2->rel_type)
		&& (edge1->e_from->get_tid() == edge2->e_from->get_tid())
		&& (edge1->e_to->get_tid() == edge2->e_to->get_tid());
}

void TraceForLearn::divide_edges()
{
	for (auto element: inspect_edges) {
		bool found = false;
		for (auto leader_element : edge_categories) {
			Edge *leader = leader_element.first;
			if (similar(leader, element.first) == true) {
				edge_categories[leader].push_back(element.first);
				found = true;
			}
		}
		if (found == false) {
			std::vector<Edge *> l;
			edge_categories[element.first] = l;
			l.push_back(element.first);
		}
	}
	LOG_S(INFO) << "Total checking edges " << edge_categories.size() << std::endl;

	std::ofstream out("suspecious_categories.log");
	out << "Total edges = " << std::dec << inspect_edges.size() << std::endl;
	out << "Total categories = " << std::dec << edge_categories.size() << std::endl;
	int index = 0;
	for (auto element: edge_categories) {
		out << "[" << std::dec << index << "]" << std::endl;
		print_edge(element.first, out);
	}
}

void TraceForLearn::tfl_pair(Node *from_node, Node *to_node, std::ofstream &out)
{
	if (from_node == nullptr || to_node == nullptr) {
		std::cout << "Invalid input nodes" << std::endl;
		return;
	}
	Group *from = from_node->get_group();
	Group *to = to_node->get_group();	
	for (auto event : from->get_container()) {
		event->EventBase::tfl_event(out);
		out << std::endl;
	}

	out << "-" << std::endl;
	for (auto event : to->get_container()) {
		event->EventBase::tfl_event(out);
		out << std::endl;
	}
	out << "=" << std::endl;
}

void TraceForLearn::tfl_pair_to_file(uint64_t from_id, uint64_t to_id, std::string path)
{
	Node *from = search_engine->id_to_node(from_id);
	Node *to = search_engine->id_to_node(to_id);
	if (!from || !to) {
		LOG_S(INFO) << "invalid pair" << std::endl;
		return;
	}

	std::ofstream out(path, std::fstream::app);
	tfl_pair(from, to, out);
}


void TraceForLearn::tfl_edges(std::set<Edge *> edges, std::string path)
{
    for (auto edge : edges)
        tfl_pair_to_file(edge->from->get_gid(), edge->to->get_gid(), path);
}


void TraceForLearn::tfl_edge_sequences(std::string path)
{
	std::string positive_pair = path + "_positive";
	std::string negtive_pair = path + "_negtive";
	uint64_t max_pattern = 300; 
	tfl_edge_positive(positive_pair, &max_pattern);
	tfl_edge_negtive(negtive_pair, max_pattern);
}

void TraceForLearn::tfl_edge_positive(std::string path, uint64_t *max_pattern)
{
	std::ofstream out_pos(path);
	assert(max_pattern != nullptr);

	out_pos << "=" << std::endl;
	uint64_t positive_pattern = 0;
	for (auto element: inspect_edges) {
		if (element.second == true) {
			tfl_pair(element.first->from, element.first->to, out_pos);
			positive_pattern++;
			if (positive_pattern >= *max_pattern)
				break;
		}
	}
	*max_pattern = positive_pattern;
	out_pos.close();
}

void TraceForLearn::tfl_edge_negtive(std::string path, uint64_t max_patterns)
{
	std::ofstream out_neg(path);
	for (auto element: inspect_edges) {
		if (element.second == false) {
			tfl_pair(element.first->from, element.first->to, out_neg);
			if (--max_patterns <= 0)
				break;
		}
	}
	
	out_neg.close();
}

void TraceForLearn::examine_edge(int index)
{
	if (index >= edge_categories.size()) {
		LOG_S(INFO) << "out of range" << std::endl;
		return;
	}
	auto it = edge_categories.begin();
	std::advance(it, index);
	Edge *edge = it->first;

	std::cout << "\tEdge type" << std::dec << edge->rel_type << std::endl;

	std::cout << "\t\tfrom event" << std::endl; 
	std::cout << "\t\t";
	edge->e_from->streamout_event(std::cout);
	std::cout << std::endl;

	std::cout << "\t\tto event" << std::endl;
	std::cout << "\t\t";
	edge->e_to->streamout_event(std::cout);
	std::cout << std::endl;
}

Edge *TraceForLearn::hueristics_examine(Node *to, int n)
{
	if (search_engine == nullptr)
		return nullptr;
	return search_engine->heuristic_edge(to, n);
}

bool TraceForLearn::validate_edge(int index, bool valid)
{
	if (index >= edge_categories.size()) {
		LOG_S(INFO) << "out of range" << std::endl;
		return false;
	}

	auto it = edge_categories.begin();
	std::advance(it, index);
	std::vector<Edge *> edges = edge_categories[it->first];
	for (auto edge : edges)
		inspect_edges[edge] = valid;
	return true;
}


#if 0
void Graph::tfl_edge_positive(std::string path, uint64_t *max_pattern)
{
	std::ofstream out_pos(path);
	assert(max_pattern != nullptr);

	if (!tfl_lists)
		generate_tfl_list();

	out_pos << "=" << std::endl;
	uint64_t positive_pattern = 0;

	for (auto edge : edges) {
		if (edge->rel_type == WEAK_REL)
			continue;
		if (!edge->from || !edge->to
			|| edge->e_from->get_procname() == "kernel_task"
			|| edge->e_to->get_procname() == "kernel_task")
			continue;

		
		tfl_pair(edge->from, edge->to, out_pos);
		positive_pattern++;
		if (positive_pattern >= *max_pattern)
			break;
	}
	out_pos.close();
	*max_pattern = positive_pattern;
}


void TraceForLearn::tfl_edge_positive(std::string path, uint64_t *max_pattern)
{
	std::ofstream out_pos(path);
	assert(max_pattern != nullptr);
	uint64_t positive_pattern = 0;

	std::map<uint64_t, std::set<Node *> > clusters;

	std::map<uint64_t, uint64_t> leader;
	for (auto node : nodes) {
		if (node->get_procname() == "kernel_task")
			continue;
		leader[node->get_gid()] = node->get_tid();
	}

	//combine-and-find
	for (auto edge: edges) {
		if(!(edge->from && edge->e_to))
			continue;
		if (!edge->from || !edge->to
			|| edge->e_from->get_procname() == "kernel_task"
			|| edge->e_to->get_procname() == "kernel_task")
			continue;
		uint64_t from = edge->from->get_gid();
		uint64_t to = edge->to->get_gid();
		if (from == to || leader[from] == leader[to])
			continue;
		uint64_t min = from > to ? to : from;
		leader[from] = leader[to] = leader[min];
	}

	for (auto edge: edges) {
		if (clusters.find(leader[min]) == clusters.end()) {
			std::set<Node *> new_set;
			clusters[leader[min]] = new_set;
		} else {
			Node *to_1 = edge->from;
			Node *to_2 = edge->to;
			std::set<std::string> to_peers_1 = edge->from->get_group()->get_group_peer();
			std::set<std::string> to_peers_2 = edge->to->get_group()->get_group_peer();

			for (auto from : clusters[leader[min]]) {

				if (to_peers_1.count(from->get_procname()) == 0) {
					CriticalPath path(graph, from, to_1);
					if (path.check_reachable() == true) {
						tfl_pair(from, to_1, out_pos);
						positive_pattern++;
					}
				}
				if (to_peers_2.count(from->get_procname()) == 0) {
					CriticalPath path(graph, from, to_2);
					if (path.check_reachable() == true) {
						tfl_pair(from, to_2, out_pos);
						positive_pattern++;
					}
				}
			}
		}
		clusters[leader[min]].insert(edge->from);
		clusters[leader[min]].insert(edge->to);
	}

	out_pos.close();
	*max_pattern = positive_pattern;
}

void TraceForLearn::tfl_edge_negtive(std::string path, uint64_t max_patterns)
{
	std::ofstream out_neg(path);

	std::map<uint64_t, uint64_t> leader;
	for (auto node : nodes) {
		if (node->get_procname() == "kernel_task")
			continue;
		leader[node->get_gid()] = node->get_tid();
	}

	//combine-and-find
	for (auto edge: edges) {
		if(!(edge->from && edge->e_to))
			continue;
		if (!edge->from || !edge->to
			|| edge->e_from->get_procname() == "kernel_task"
			|| edge->e_to->get_procname() == "kernel_task")
			continue;
		uint64_t from = edge->from->get_gid();
		uint64_t to = edge->to->get_gid();
		if (from == to || leader[from] == leader[to])
			continue;
		uint64_t min = from > to ? to : from;
		leader[from] = leader[to] = leader[min];
	}

	while (!leader.empty()) {
		uint64_t from_index = leader.begin()->first;
		Node *from = graph->id_to_node(from_index);
		if (from == nullptr) {
			leader.erase(from_index);
			continue;
		}

		std::set<std::string> from_peers = from->get_group()->get_group_peer();
		for (auto element : leader) {
			if (element.second == leader[from_index])
				continue;

			Node *to = graph->id_to_node(element.first);
			if (to == nullptr)
				continue;
			
			if (from_peers.count(to->get_procname()) == 0)
				continue;

			tfl_pair(from, to, out_neg);

			if (--max_patterns == 0)
				goto out;
		}
		leader.erase(from_index);
	}
out:
	out_neg.close();
}
#endif
