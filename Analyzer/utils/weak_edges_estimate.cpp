#include "weak_edges_estimate.hpp"
WeakEdgeCalculator::WeakEdgeCalculator(Graph *graph)
:graph_ptr(graph)
{
	assert(graph);
	parse_applog_events(graph->get_list_of_applogs());
}

void WeakEdgeCalculator::parse_applog_events(std::list<EventBase *> &applogs)
{
	if (applogs.size() == 0) {
		LOG_S(INFO) << "No app events recorded for validation" << std::endl;
		return;
	}
	std::unordered_map<tid_t, std::stack<AppLogEvent *> >event_stacks;

	for (auto event : applogs) {
		AppLogEvent *applog_event = dynamic_cast<AppLogEvent *>(event);
		assert(applog_event);
		std::string desc = applog_event->get_desc();
		tid_t tid = applog_event->get_tid();

#if DOM
		//processing when dom_loading/dom_complete
		//skip other events 
		if (desc == "dom_loading") {
#else
		std::string state = applog_event->get_state();
		if (state == "begin") {
#endif
			if (event_stacks.find(tid) == event_stacks.end()) {
				std::stack<AppLogEvent *> stack;
				event_stacks[tid] = stack;
			}
			event_stacks[tid].push(applog_event);
		}



#if DOM
		if (desc == "dom_complete") {
#else
		if (state == "end") {
#endif
			if (event_stacks.find(tid) == event_stacks.end())
				continue;
			if (event_stacks[tid].empty())
				continue;

			AppLogEvent *match_event = event_stacks[tid].top();
			assert(match_event);
			//if (match_event->get_prefix() == applog_event->get_prefix()) {
			event_stacks[tid].pop();
			//if not in the same group, try the weak edge exploring.
			if (graph_ptr->group_of(applog_event) != nullptr
				&& graph_ptr->group_of(match_event) != graph_ptr->group_of(applog_event)) {
				int ret = weak_edge_collect(match_event, applog_event);
				LOG_S(INFO) << "#Weak Edges = " << std::dec << ret << std::endl;
				if (max_count <= ret)
					break;
			} else {
				//TODO
				//check if a mismatch of event in the stack top,
				//other wise discard the event and throw error messages.
			}
		}
	}//for all applog events
}

int WeakEdgeCalculator::weak_edge_collect(EventBase *begin, EventBase *end)
{
	//search the path
	//collect weak edges
	int count = 0;
	CriticalPath path(graph_ptr, graph_ptr->node_of(begin), graph_ptr->node_of(end),
                    begin, end);
	if (path.check_reachable() == false)
		return weak_edges.size();
	for (auto &elem : path.get_path()) {
		if (elem->is_weak()) {
			if (elem->in_edge() != nullptr) {
				weak_edges.insert(elem->in_edge());			
				count++;
			}
			continue;
		}

		if (elem->node()!= nullptr && count > 0) {
			auto edges = elem->node()->get_in_weak_edges();
			for (auto edge_elem : edges) {
				invalid_weaks.insert(edge_elem.second);
				count--;
			}
		}
	}
	return weak_edges.size();
}
