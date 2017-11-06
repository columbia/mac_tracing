#ifndef BUG_SEARCH_HPP
#define BUG_SEARCH_HPP
#include "cluster.hpp"

class BugSearcher {
	map<uint64_t, cluster_t *> cluster_map;
	groups_t *groups_ptr;
	event_t *ceiling_event;
	event_t *floor_event;
	
	event_t *search_source_event_in_main_thread(int event_type, event_t *event);
	cluster_t *search_cluster_overlap(event_t *event);
public:
	BugSearcher(clusters_t *cluster_gen);
	cluster_t *report_spinning_cluster();
	template <typename T>
	T max_item_key(map<T, double> &maps, T comb);
	wait_ev_t *suspicious_blocking(cluster_t *cluster);
	group_t *suspicious_segment(cluster_t *cluster);
};
#endif
