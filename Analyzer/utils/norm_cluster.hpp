//////////////////////////////////////////////
#ifndef NORM_CLUSTER_HPP
#define NORM_CLUSTER_HPP

#include "cluster.hpp"
#include "canonization.hpp"

class NormCluster {
	cluster_t *cluster;
	map<uint64_t, bool> &key_events;
	vector<norm_group_t *> sort_vertexes;
	map<group_t *, norm_group_t *> vertexes_map;
	map<norm_group_t *, group_t *> vertexes_reverse_map;

	void init_vertex_degrees(vector<rel_t> &edges);
	void create_vertexes(vector<group_t *> &nodes);

	static bool compare_vertexes_in(norm_group_t *elem1, norm_group_t *elem2);
	norm_group_t *sort_vertexes_in(vector<norm_group_t *>::iterator begin, vector<norm_group_t *>::iterator end);
	void remove_degree_edges_from(norm_group_t *group, node_edges_map_t &group_inedges_map, node_edges_map_t &group_outedges_map);
	void virtualize_tid_for_groups(void);

public:
	NormCluster(cluster_t *c, map<uint64_t, bool> &key_events);
	~NormCluster();

	uint64_t original_size() {return cluster->get_nodes().size();}
	uint64_t original_edges() {return cluster->get_edges().size();}
	uint64_t get_cluster_id() {return cluster->get_cluster_id();}

	void topological_sort();
	vector<norm_group_t *> &get_sort_vertexes(void) {return sort_vertexes;}

	bool operator==(NormCluster &other);
	bool operator!=(NormCluster &other);
	void decode_cluster(ofstream &output);
};
typedef NormCluster norm_cluster_t;

///////////////////////////////////////////////
class Normalizer {
	map<uint64_t, cluster_t *> &clusters;
	/* pick key events for normalization */
	map<uint64_t, bool> key_events;
	map<uint64_t, norm_cluster_t *> norm_clusters;
	vector<norm_cluster_t *> outstanders;

	/*nomailization*/
	void normalize_cluster(uint64_t index, cluster_t *cluster);

public:
	Normalizer(map<uint64_t, cluster_t *> &clusters);
	~Normalizer();
	void normalize_clusters(void);
	void sort_clusters(void);
	map<uint64_t, norm_cluster_t *> &get_normed_clusters(void) {return norm_clusters;}
	void compare(Normalizer* base, const char *outfile);
	void decode_outstand_cluster(const char *outfile);
};
typedef Normalizer normalizer_t;

#endif
