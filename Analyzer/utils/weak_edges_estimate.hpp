#ifndef WEAK_EDGE_ESTIMATE
#define WEAK_EDGE_ESTIMATE

#include "critical_path.hpp"
#include "applog.hpp"
#include "graph.hpp"
#include <stack>

class WeakEdgeCalculator {
	Graph *graph_ptr;
	std::set<Edge *> weak_edges;
    std::set<Edge *> invalid_weaks;
    static const int max_count = 1000;
    
	void parse_applog_events(std::list<EventBase *> &applogs);
	int weak_edge_collect(EventBase *begin, EventBase *end);

public:
	WeakEdgeCalculator(Graph *graph);
    std::set<Edge *> &get_positive_weaks() {return weak_edges;}
    std::set<Edge *> &get_negative_weaks() {return invalid_weaks;}
};
#endif
