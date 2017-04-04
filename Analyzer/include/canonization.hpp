#ifndef CANONIZATION_HPP
#define CANONIZATION_HPP

#include "cluster.hpp"
#include <math.h>

class NormEvent {
	string proc_name;
	string optype;
	uint64_t virtual_tid;
	/* refer to original event */
	event_t * event;

public:
	NormEvent(event_t * ev, uint64_t virtual_tid);
	bool operator==(NormEvent & other);
	bool operator!=(NormEvent & other);
	event_t * get_real_event(void);
};
typedef NormEvent norm_ev_t;

//////////////////////////////////////////////
class NormGroup {
	/* pick key events for normalization */
	map<uint64_t, bool> key_events;
	list<norm_ev_t*> container;
	uint64_t virtual_tid;
	uint32_t in;
	uint32_t out;
	uint64_t compressed;
	bool is_norm;
	double delta;
	/* reference to original group */
	group_t * group;

	void normalize_events(void);
	uint32_t check_event_sequence(list<norm_ev_t *>::iterator begin, int distance);
	void compress_group(void);
	
public:
	NormGroup(Group *g, uint64_t vtid);
	~NormGroup();
	void add_in_edge(void) {in++;}
	void add_out_edge(void) {out++;}
	void check_compress(void);
	uint64_t original_size(void) {return group->get_container().size();}
	uint64_t get_group_id(void) {return group->get_group_id();}
	list<norm_ev_t*> & get_container(void) {return container;}
	map<string, uint32_t> & get_group_tags(void) {return group->get_group_tags();}
	void set_norm(void) {is_norm = true;}
	bool check_norm(void) {return is_norm;}
	double get_delta(void) {return delta;}
	bool operator==(NormGroup & other);
	bool operator!=(NormGroup & other);
	void decode_group(ofstream & output);
};
typedef NormGroup norm_group_t;

//////////////////////////////////////////////
#define NSEVENT_THREAD	0UL
#define MAIN_THREAD		~0UL

class NormCluster {
	vector<norm_group_t*> norm_nodes;
	map<event_t *, norm_ev_t *> elements;
	map<group_t *, norm_group_t *> vertexes;
	map<uint64_t, uint64_t> vtid_map;
	cluster_t * cluster;
	uint64_t compressed;

	uint64_t check_special_thread(group_t * cur_group, uint64_t spec_thread);
	void assign_virtual_tid(void);
	void normalize_nodes(vector<group_t *> &nodes);
	void collect_elements_from_group(norm_group_t *);
	void normalize_edges(vector<rel_t> &edges);

	uint32_t check_group_sequence(vector<norm_group_t *>::iterator begin, int step, vector<norm_group_t *>::iterator end);
	void compress_cluster(void);

	void topological_sort();

public:
	NormCluster(cluster_t *c);
	~NormCluster();
	bool is_ground(void) {return cluster->check_ground();}
	bool is_compressed(void) {return compressed != 0;}
	void check_compress(void);
	uint64_t complexity(void) {return original_size() << 32 | original_edges();}
	uint64_t original_size() {return cluster->get_nodes().size();}
	uint64_t original_edges() {return cluster->get_edges().size();}
	uint64_t get_cluster_id() {return cluster->get_cluster_id();}
	vector<norm_group_t *> & get_norm_nodes(void) {return norm_nodes;}
	bool operator==(NormCluster & other);
	bool operator!=(NormCluster & other);
	void decode_cluster(ofstream &output);
};
typedef NormCluster norm_cluster_t;

///////////////////////////////////////////////
class Normalizer {
	map<uint64_t, norm_cluster_t *> norm_clusters;
	map<uint64_t, list<norm_cluster_t *> > complex_clusters;
	vector<norm_cluster_t *> outstanders;

	void normalize_cluster(uint64_t index, cluster_t * cluster);
	void check_compress(void);

	void check_group(norm_group_t * group, vector<norm_group_t *> & base_groups);
	void divide_by_complexity(void);
	void check_by_complexity(list<norm_cluster_t *> & cluster_list, ofstream & output);

public:
	Normalizer(map<uint64_t, cluster_t *> &clusters);
	~Normalizer();
	map<uint64_t, norm_cluster_t *> & get_normed_clusters(void) {return norm_clusters;}
	vector<norm_group_t *> get_groups(void);
	void compare_groups(Normalizer * base);
	void inner_compare(const char  * outputfile);
	void compare(Normalizer* base, const char * outputfile);
	void decode_outstand_cluster(const char * outputfile);
};
typedef Normalizer normalizer_t;

#endif
