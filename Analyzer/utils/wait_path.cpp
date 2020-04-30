#include "wait_path.hpp"
Node *WaitInterval::begin_node()
{
	if (graph == nullptr)
		return nullptr;
	return graph->node_of(wait);
}

Node *WaitInterval::end_node()
{
	if (graph == nullptr)
		return nullptr;
	return graph->node_of(wake);
}

bool WaitInterval::fill_path()
{
	sub_path = new CriticalPath(graph, begin_node(), end_node());
	return sub_path->check_reachable();
}

FillPath::FillPath(Graph *graph_, EventBase *begin_, EventBase *end_)
:graph(graph_)
{
	assert(begin_ && end_);
	assert(begin_->get_tid() == end_->get_tid());
	std::map<group_id_t, Group *> &groups = graph->get_groups_ptr()->get_groups_by_tid(end_->get_tid());
	Group *begin = graph->group_of(begin_);
	Group *end = graph->group_of(end_);
	time_total = end_->get_abstime() - begin_->get_abstime();

	auto it = groups.begin();
	Group *prev_group = nullptr, *cur_group = nullptr;
	wait_total = 0.0;

	while (it != groups.end() && it->second != begin) {
		cur_group = it->second;
		main_path.push_back(cur_group);	
		if (prev_group) {
			assert(cur_group->get_group_id() > prev_group->get_group_id());
			EventBase *last_event = prev_group->get_last_event();
			EventBase *next_event = cur_group->get_first_event();
			if (last_event->get_event_type() == WAIT_EVENT
				&& next_event->get_event_type() == FAKED_WOKEN_EVENT) {
				WaitInterval *interval = new WaitInterval(graph, last_event, next_event);
				double cur_wait = next_event->get_abstime() - last_event->get_abstime();
				interval->set_wait_time(cur_wait);
				waits[cur_group] = interval;
				wait_total += cur_wait;
			}
		}
		it++;
		if (cur_group == end)
			break;

		prev_group = cur_group;
	}

	double wait_ratio = wait_total / time_total;
	std::cout << "Wait/Total time ratio = " << std::fixed << std::setprecision(1)\
		<< wait_ratio << std::endl;
	if (wait_ratio <= 0.1)
		return;
	else {
		fill_wait_intervals();
		save_path_to_file("wait_path.log");
	}
}

void FillPath::fill_wait_intervals()
{
	for (auto element : waits) {
		if (element.second->get_wait_time() > wait_total / waits.size()) {
			element.second->fill_path();
		}
	}
}

FillPath::~FillPath()
{
	for (auto element : waits) {
		if (element.second != nullptr)
			delete element.second;
	}
	waits.clear();
	main_path.clear();
}

void FillPath::save_path_to_file(std::string log)
{
	/*
	std::ofstream out_file(log, std::ios::app);
	for (auto element : main_path) {
		element->streamout_group(out_file);
		if (waits.find(element) != waits.end()) {
			out_file << "<<<<<<<<<<\n";
			waits[element]->save_path(out_file);
			out_file << ">>>>>>>>>>\n";
		}
	}
	out_file.close();
	*/
}
