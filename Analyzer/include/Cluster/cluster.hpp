#ifndef CLUSTER_HPP
#define CLUSTER_HPP
#include "group.hpp"

#define MSGP_REL	 0
#define MKRUN_REL	 1
#define DISP_EXE_REL 2
#define DISP_DEQ_REL 3
#define CALLOUT_REL	 4
#define CALLOUTCANCEL_REL 5
#define CA_REL		6
#define BRTRAP_REL	7

struct rel_t {
	group_t *g_from;
	group_t *g_to;
	event_t *e_from;
	event_t *e_to;
	uint32_t rel_type;
	bool operator==(const rel_t & rhs) const
	{
		return (e_from == rhs.e_from 
			&& e_to == rhs.e_to 
			&& rel_type == rhs.rel_type);
	}
};

typedef map<mkrun_ev_t*, list<event_t *>::iterator> mkrun_pos_t;
typedef multimap<group_t *, rel_t> node_edges_map_t;
typedef pair<multimap<group_t *, rel_t>::iterator, multimap<group_t *, rel_t>::iterator> multimap_range_t;

class Cluster {
	uint64_t cluster_id;
	vector<rel_t> edges;
	vector<group_t *> nodes;

	bool is_ground;
	bool is_infected;
	vector<group_t *> infected_groups;
	vector<group_t *> gt_groups;

	group_t * root;
	list<event_t *>connectors;
	list<wait_ev_t *>wait_events;

	void js_edge(ofstream &outfile, event_t *host, const char *action, event_t *peer, bool *comma);
	void message_edge(ofstream &outfile, event_t *event, bool *);
	void dispatch_edge(ofstream &outfile, event_t *event, bool *);
	void timercall_edge(ofstream &outfile, event_t *event, bool *);
	void mkrun_edge(ofstream &outfile, event_t *event, bool *);
	void wait_label(ofstream &outfile, event_t *event, bool *);
	void coreannimation_edge(ofstream &outfile, event_t *event, bool *);

	void js_arrows(ofstream &outfile);
	void js_lanes(ofstream &outfile);
	void js_groups(ofstream &outfile);

public:
	Cluster(void);
	Cluster(group_t *);
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
	vector<group_t *> &get_infected_groups(void) {return infected_groups;}
	vector<group_t *> &get_gt_groups(void) {return gt_groups;}

	void push_connectors(group_t *, event_t *);
	list<event_t *> pop_cur_connectors();
	list<wait_ev_t *> &get_wait_events();

	static bool compare_time(group_t *, group_t *);
	static bool compare_edge_from(rel_t edge1, rel_t edge2);
	void sort_nodes();
	void sort_edges();
	void classify_cluster_edges(node_edges_map_t &to_edges, node_edges_map_t &from_edges);
	int get_node_idx_in_cluster(group_t *node);
	rel_t direct_edge(string from_proc, string to_proc);
	bool traverse(string to_proc, group_t *cur_node, bool *visited, node_edges_map_t &from_edges,
			node_edges_map_t &to_edges, vector<rel_t> &paths, map<pair<group_t *, string>, vector<rel_t> > &sub_result);
	void search_paths(string from_proc, string to_proc, ofstream &outfile, map<pair<group_t *, string>, vector<rel_t> > &sub_result);
	void inspect_procs_irrelevance(ofstream &outfile);


	void js_cluster(ofstream &outfile);
	void decode_edges(ofstream &outfile);
	void decode_cluster(ofstream &outfile);
	void streamout_cluster(ofstream &outfile);
};
typedef Cluster cluster_t;

class Clusters {
	list<event_t*> evlist;
	groups_t *groups_ptr;
	map<uint64_t, cluster_t *> clusters;
	uint64_t index;

	cluster_t *create_cluster();
	bool del_cluster(cluster_t *);
	cluster_t *cluster_of(group_t *);

	cluster_t *merge(cluster_t*, cluster_t*);
	void merge_clusters_of_events(event_t *, event_t *, uint32_t);

public:
	Clusters(groups_t *groups_ptr);
	~Clusters(void);

	void merge_by_mach_msg(void);
	void merge_by_dispatch_ops(void);
	void merge_by_mkrun(void);
	void merge_by_timercallout(void);
	void merge_by_ca(void);
	void merge_by_breakpoint_trap(void);

	uint64_t get_size(void) { return clusters.size();}
	map<uint64_t, cluster_t *>& get_clusters() {return clusters;}
	void decode_clusters(string & output_path);
	void streamout_clusters(string & output_path);
	void decode_dangle_groups(string & output_path);
};
typedef Clusters clusters_t;

class ClusterGen {
	groups_t *groups_ptr;
	map<uint64_t, cluster_t *> clusters;
	cluster_t *spin_cluster;

	cluster_t *init_cluster(group_t *group);
	void augment_cluster(cluster_t *cluster);

	void merge_by_mach_msg(cluster_t *, msg_ev_t *);
	void merge_by_dispatch_ops(cluster_t *, enqueue_ev_t *);
	void merge_by_dispatch_ops(cluster_t *, dequeue_ev_t *);
	void merge_by_timercallout(cluster_t *, timercreate_ev_t *);
	void merge_by_coreanimation(cluster_t *cluster, ca_set_ev_t *cadisplay_event);
	void merge_by_sharevariables(cluster_t *cluster, breakpoint_trap_ev_t *breakpoint_event);
	void merge_by_waitsync(cluster_t *cluster, wait_ev_t *wait_event);
	void add_waitsync(cluster_t *cluster);

public:
	ClusterGen(groups_t *groups_ptr);
	~ClusterGen(void);
	void streamout_clusters(string & output_path);
	void js_clusters(string & output_path);
	void inspect_clusters(string &output_path);
};

#endif
