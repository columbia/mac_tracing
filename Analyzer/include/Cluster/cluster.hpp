#ifndef CLUSTER_HPP
#define CLUSTER_HPP
#include "group.hpp"

#define MSGP	0
#define MKRUN	1
#define DISEXE	2
#define CALLOUT 3
#define CALLOUTCANCEL 4

struct rel_t {
	group_t * g_from;
	group_t * g_to;
	event_t * e_from;
	event_t * e_to;
	uint32_t rel_type;
	bool operator==(const rel_t & rhs) const
	{
		return (e_from == rhs.e_from 
			&& e_to == rhs.e_to 
			&& rel_type == rhs.rel_type);
	}
};

class Cluster {
	uint64_t cluster_id;
	vector<rel_t> edges;
	vector<group_t *> nodes;
	bool is_ground;
	bool is_infected;
	vector<group_t *> infected_groups;
	vector<group_t *> gt_groups;

public:
	Cluster(void);
	~Cluster();
	void add_edge(group_t*, group_t*, event_t *, event_t *, uint32_t);
	void add_node(group_t*);
	vector<group_t*>& get_nodes(void) {return nodes;}
	vector<rel_t>& get_edges(void) {return edges;}
	bool remove_edge(const rel_t edge);
	bool remove_node(group_t * node);
	void set_cluster_id(uint64_t _cluster_id) {cluster_id = _cluster_id; }
	uint64_t get_cluster_id() {return cluster_id;}
	void append_nodes(vector<group_t *> &);
	void append_edges(vector<rel_t> &);
	bool check_ground(void) {return is_ground;}
	bool check_infected(void) {return is_infected;}
	vector<group_t *> & get_infected_groups(void) {return infected_groups;}
	vector<group_t *> & get_gt_groups(void) {return gt_groups;}
	static bool compare_time(group_t *, group_t*);
	void sort_nodes();
	void decode_cluster(ofstream &outfile);
	void decode_edges(ofstream &outfile);
	void streamout_cluster(ofstream &outfile);
	//void clear_cluster();
};

typedef Cluster cluster_t;

class Clusters {
	list<event_t*> evlist;
	groups_t * groups_ptr;
	map<uint64_t, cluster_t *> clusters;
	uint64_t index;

	cluster_t * create_cluster();
	bool del_cluster(cluster_t *);
	cluster_t * cluster_of(group_t *);

	cluster_t * merge(cluster_t*, cluster_t*);
	void para_connector_generate(void);
	void merge_clusters_of_events(event_t *, event_t *, uint32_t);

public:
	Clusters(groups_t * groups_ptr);
	~Clusters(void);

	void merge_by_mach_msg(void);
	void merge_by_dispatch_ops(void);
	void merge_by_mkrun(void);
	void merge_by_callout(void);

	uint64_t get_size(void) { return clusters.size();}
	map<uint64_t, cluster_t *>& get_clusters() {return clusters;}
	void decode_clusters(string & output_path);
	void streamout_clusters(string & output_path);
	void decode_dangle_groups(string & output_path);
	//void clear_clusters(void);
};
typedef Clusters clusters_t;

#endif
