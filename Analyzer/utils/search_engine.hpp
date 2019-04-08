#ifndef BUG_SEARCH_HPP
#define BUG_SEARCH_HPP
#include "cluster.hpp"
#include "canonization.hpp"

typedef uint64_t thread_id_t;
typedef uint64_t group_id_t;

class BugSearcher {
	map<uint64_t, cluster_t *> cluster_map;
	groups_t *groups_ptr;
	map<thread_id_t, map<group_id_t, norm_group_t*> > normalized_groups_map;
	event_t *ceiling_event;
	event_t *floor_event;
	
	map<uint64_t, bool> add_key_events();
	event_t *search_source_event_in_main_thread(int event_type, event_t *event);
	cluster_t *search_cluster_overlap(event_t *event);

public:
	BugSearcher(clusters_t *cluster_gen);
	BugSearcher(groups_t *groups);
	~BugSearcher() {cluster_map.clear(); groups_ptr = NULL; ceiling_event = floor_event = NULL;};
	cluster_t *report_spinning_cluster();
	template <typename T>
	T max_item_key(map<T, double> &maps, T comb);
	wait_ev_t *suspicious_blocking(cluster_t *cluster);
	map<wait_ev_t*, double> suspicious_blocking(groups_t *groups, string outfile);
	group_t *suspicious_segment(cluster_t *cluster);
	vector<uint64_t> get_counterpart(uint64_t group_id, thread_id_t thread);
	void update_normalized_map_for_thread(thread_id_t thread);
	void slice_path(uint64_t group_id, string outfile);
};
#endif

