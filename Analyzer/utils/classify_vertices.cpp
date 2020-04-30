#include "classify_vertices.hpp"
#include "loguru.hpp"

VerticesClass::VerticesClass(Graph *_graph)
{
	graph = _graph;
	vertice_map.clear();
	classify_vertices();
}

std::string construct_key(Node *node)
{
	std::string ret;
	std::set<int> events;
	for (auto event: node->get_group()->get_container()) {
		if (event->get_event_type() == MR_EVENT 
			|| event->get_event_type() == INTR_EVENT)
			continue;
		events.insert(event->get_event_type());
	}
	
	for (auto item: events) {
		ret += std::to_string(item);
		ret += "_";
	}

	return ret;
}

uint64_t VerticesClass::classify_vertices()
{
	for (auto node: graph->get_nodes()) {
		std::string key = construct_key(node);
		if (vertice_map.find(key) == vertice_map.end()) {
			std::vector<uint64_t> list;
			vertice_map[key] = list;
		}
		vertice_map[key].push_back(node->get_gid());
	}
	return vertice_map.size();
}

void VerticesClass::render_statistics()
{
	for (auto elem: vertice_map) {
		LOG_S(INFO) << elem.first << "\n\t";
		for (auto index : elem.second) {
			LOG_S(INFO) << std::hex << index << "\t";
		}
		LOG_S(INFO) << "\n";
	}
	LOG_S(INFO) << "\n";
}
