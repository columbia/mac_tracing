#ifndef PATH_EXTRACTOR_HPP
#define PATH_EXTRACTOR_HPP
#include "cluster.hpp"

#define PATH_FILTER		0
#define PROCESS_FILTER	1

class Filter {
	map<uint64_t, cluster_t *> original_clusters;
	map<uint64_t, cluster_t *> filtered_clusters;
	set<string> process_set;
	
	//typedef multimap<group_t *, rel_t> node_edges_map_t;
	//typedef pair<multimap<group_t *, rel_t>::iterator, multimap<group_t *, rel_t>::iterator> multimap_range_t;

	void select_clusters(clusters_t * clusters);
	void sort_cluster_edges(vector<rel_t> &cluster_edges, node_edges_map_t & to_edges, node_edges_map_t & from_edges);
	int get_node_idx_in_cluster(group_t * node, cluster_t * cluster);
	int back_traverse_from(group_t *infected_group, bool * visited, cluster_t * cur_cluster, node_edges_map_t &to_edges, cluster_t * dst_cluster);
	cluster_t * filter_by_traverse(int index, cluster_t *cur_cluster);

	bool read_processes(string filename);
	bool erase_edge_in_range(rel_t, multimap_range_t &, node_edges_map_t &);
	bool audit_deletion_of_node(group_t *cur_node, node_edges_map_t &from_edges, node_edges_map_t &to_edges);
	bool audit_deletion_of_node(group_t *cur_node, node_edges_map_t &to_edges);
	void remove_node(cluster_t * cur_cluster, group_t * cur_node, node_edges_map_t &to_edges, node_edges_map_t &from_edges);
	cluster_t * filter_by_proc_intersection(cluster_t * cluster);

public:
	Filter(clusters_t *clusters);
	~Filter();
	void clusters_filter_para();
	void clusters_filter_para(uint64_t type);
	void streamout_buggy_clusters(string output_path);
	void streamout_filtered_clusters(string output_path);
	void decode_filtered_clusters(string output_path);
	void decode_edges(string output_path);
	map<uint64_t, cluster_t *> & get_filtered_clusters(void) {return filtered_clusters.size() ? filtered_clusters: original_clusters;}
};
typedef Filter cluster_filter_t;

#endif
