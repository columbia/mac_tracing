#ifndef WAIT_FILL_PATH_HPP
#define WAIT_FILL_PATH_HPP
#include "graph.hpp"
#include "critical_path.hpp"

class WaitInterval
{
	Graph *graph;
	EventBase *wait;
	EventBase *wake;
	double wait_time;
	//std::vector<Element *> path;
	CriticalPath *sub_path;
public:
	WaitInterval(Graph *graph_, EventBase *wait_, EventBase *wake_)
	:graph(graph_), wait(wait_), wake(wake_) {sub_path = nullptr;}
	~WaitInterval() {if (sub_path) delete sub_path; sub_path = nullptr;}
	Node *begin_node();
	Node *end_node();
	void set_wait_time(double wait_time_) {wait_time = wait_time_;}
	double get_wait_time() {return wait_time;}
	bool fill_path();
	std::vector<Element *> get_path() {std::vector<Element *> empty; if (sub_path) return sub_path->get_path(); return empty;}
	void save_path(std::ofstream &out) {return sub_path->save_path(out);}
};

class FillPath
{
private:
	Graph *graph;
	//EventBase *begin;
	//EventBase *end;
	double wait_total;
	double time_total;
	std::map<Group *, WaitInterval *> waits;
	std::vector<Group *> main_path;
	//std::vector<WaitInterval *> waits;
public:
	FillPath(Graph *graph_, EventBase *begin_, EventBase *end_);
	~FillPath();
	void fill_wait_intervals();
	void save_path_to_file(std::string log);
};
#endif
