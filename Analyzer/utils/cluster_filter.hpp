#ifndef PATH_EXTRACTOR_HPP
#define PATH_EXTRACTOR_HPP
#include "cluster.hpp"

#define PATH_FILTER		0x1UL
#define PROCESS_FILTER	0x1UL << 1

class Filter {
	clusters_t *clusters_ptr;
	map<uint64_t, cluster_t *> original_clusters;
	map<uint64_t, cluster_t *> filtered_clusters;
	set<string> process_set;
	bool path_filter;
	bool process_filter;

	void select_clusters(clusters_t *cluster_ptr);
	int back_traverse_from(group_t *colored_group, bool *visited, cluster_t *cur_cluster, node_edges_map_t &to_edges, cluster_t *dst_cluster);
	cluster_t *filter_by_traverse(int index, cluster_t *cur_cluster);

	bool read_processes(string filename);
	bool erase_edge_in_range(rel_t, multimap_range_t &, node_edges_map_t &);
	bool audit_deletion_of_node(group_t *cur_node, node_edges_map_t &edges);
	void remove_node(cluster_t *cur_cluster, group_t *cur_node, node_edges_map_t &to_edges, node_edges_map_t &from_edges);
	cluster_t *filter_by_proc_intersection(cluster_t *cluster);

	void clusters_filter_para(uint64_t type);

public:
	Filter(clusters_t *clusters_ptr);
	~Filter();

	void clusters_filter(uint64_t types);

	void streamout_filtered_clusters(string output_path);
	void decode_filtered_clusters(string output_path);
	void decode_edges(string output_path);
	map<uint64_t, cluster_t *> &get_filtered_clusters(void) {return filtered_clusters.size() ? filtered_clusters: original_clusters;}
};
typedef Filter cluster_filter_t;

#endif
