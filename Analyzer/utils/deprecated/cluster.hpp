#ifndef CLUSTER_HPP
#define CLUSTER_HPP
#include "group.hpp"

#define MSGP_REL     0
#define MKRUN_REL     1
#define DISP_EXE_REL 2
#define DISP_DEQ_REL 3
#define CALLOUT_REL     4
#define CALLOUTCANCEL_REL 5
#define CA_REL        6
#define BRTRAP_REL    7
#define RLITEM_REL    8

struct rel_t{
    Group *g_from;
    Group *g_to;
    EventBase *e_from;
    EventBase *e_to;
    uint32_t rel_type;
    bool operator==(const rel_t &rhs) const
    {
        return (e_from == rhs.e_from 
            && e_to == rhs.e_to 
            && rel_type == rhs.rel_type);
    }
};
typedef multistd::map<Group *, rel_t> node_edges_map_t;
typedef std::pair<multistd::map<Group *, rel_t>::iterator, multistd::map<Group *, rel_t>::iterator> multimap_range_t;

class Cluster {
    uint64_t cluster_id;
    std::vector<rel_t> edges;
    std::vector<Group *> nodes;

    Group *root;
    std::list<EventBase *>connectors;
    std::list<WaitEvent *>wait_events;

    /* dump cluster info to java script */
    void js_edge(std::ofstream &outfile, EventBase *host, const char *action, EventBase *peer, bool *comma);
    void message_edge(std::ofstream &outfile, EventBase *event, bool *);
    void dispatch_edge(std::ofstream &outfile, EventBase *event, bool *);
    void timercall_edge(std::ofstream &outfile, EventBase *event, bool *);
    void mkrun_edge(std::ofstream &outfile, EventBase *event, bool *);
    void wait_label(std::ofstream &outfile, EventBase *event, bool *);
    void coreannimation_edge(std::ofstream &outfile, EventBase *event, bool *);
    void sharevariable_edge(std::ofstream &outfile, EventBase *event, bool *comma);
    void runloop_item_edge(std::ofstream &outfile, EventBase *event, bool *comma);

    void js_arrows(std::ofstream &outfile);
    void js_lanes(std::ofstream &outfile);
    void js_groups(std::ofstream &outfile);

    /* inspect cluster */
    rel_t direct_edge(std::string from_proc, std::string to_proc);
    bool traverse(std::string to_proc, Group *cur_node, bool *visited, node_edges_map_t &from_edges,
            node_edges_map_t &to_edges, std::vector<rel_t> &paths,std::map<std::pair<Group *, std::string>, std::vector<rel_t> > &sub_result);
    void search_paths(std::string from_proc, std::string to_proc, std::ofstream &outfile,std::map<std::pair<Group *, std::string>, std::vector<rel_t> > &sub_result);

    /* check conntions*/
    bool traverse_connection(Group *cur_node, Group *destination_node, bool *visited, node_edges_map_t &to_edges, node_edges_map_t &from_edges, std::vector<rel_t> &paths);

public:
    Cluster(void);
    Cluster(Group *);
    ~Cluster();

    void add_edge(Group *, Group *, EventBase *, EventBase *, uint32_t);
    bool add_node(Group *);
    bool remove_edge(const rel_t edge);
    bool remove_node(Group *node);
    std::vector<Group *> &get_nodes(void) {return nodes;}
    std::vector<rel_t> &get_edges(void) {return edges;}
    void append_nodes(std::vector<Group *> &);
    void append_edges(std::vector<rel_t> &);

    void set_cluster_id(uint64_t _cluster_id) {cluster_id = _cluster_id; }
    uint64_t get_cluster_id() {return cluster_id;}

    /* augment cluster */
    void push_connectors(Group *, EventBase *);
    std::list<EventBase *> pop_cur_connectors();
    std::list<WaitEvent *> &get_wait_events();

    static bool compare_time(Group *, Group *);
    static bool compare_edge_from(rel_t edge1, rel_t edge2);
    void sort_nodes();
    void sort_edges();
    void classify_cluster_edges(node_edges_map_t &to_edges, node_edges_map_t &from_edges);
    int get_node_idx_in_cluster(Group *node);
    
    void inspect_procs_irrelevance(std::ofstream &outfile);
    void compare(Cluster *cluster, std::ofstream &outfile);
    void check_connection(Group *group_1, Group *group_2, std::ofstream &output);

    void js_cluster(std::ofstream &outfile);
    void decode_edge(std::ofstream &outfile, rel_t edge);
    void decode_edges(std::ofstream &outfile);
    void decode_cluster(std::ofstream &outfile);
    void streamout_cluster(std::ofstream &outfile);
};
typedef Cluster cluster_t;

/* version0: init clusters then apply filters to trim noise */
class ClusterInit {
    std::list<EventBase *> evlist;
    Groups *groups_ptr;
    std::map<uint64_t, cluster_t *> clusters;
    uint64_t index;

    cluster_t *create_cluster();
    bool del_cluster(cluster_t *);

    cluster_t *merge(cluster_t *, cluster_t *);
    void merge_clusters_of_events(EventBase *, EventBase *, uint32_t);
    void merge_by_mach_msg(void);
    void merge_by_dispatch_ops(void);
    void merge_by_mkrun(void);
    void merge_by_timercallout(void);
    void merge_by_ca(void);
    void merge_by_breakpoint_trap(void);

public:
    ClusterInit(Groups *groups_ptr);
    ~ClusterInit(void);

    cluster_t *cluster_of(Group *);
    uint64_t get_size(void) { return clusters.size();}
    std::map<uint64_t, cluster_t *>& get_clusters() {return clusters;}
    Groups *get_groups_ptr() {return groups_ptr;}

    void streamout_clusters(std::string &output_path);
    void decode_clusters(std::string &output_path);
    void decode_dangle_groups(std::string &output_path);
};
//typedef ClusterInit clusters_t;

/*version1: directly checking the relationships among processes
 *            using inspector to correct falses
 */
class ClusterGen {
    Groups *groups_ptr;
    std::map<uint64_t, cluster_t *> clusters;

    cluster_t *init_cluster(Group *group);
    void augment_cluster(cluster_t *cluster, bool connect_mkrun);

    /*add groups into cluster via connections*/
    void merge_by_mach_msg(cluster_t *, MsgEvent *);
    void merge_by_dispatch_ops(cluster_t *, BlockEnqueueEvent *);
    void merge_by_dispatch_ops(cluster_t *, BlockDequeueEvent *);
    void merge_by_timercallout(cluster_t *, TimerCreateEvent *);
    void merge_by_coreanimation(cluster_t *cluster, CASetEvent *cadisplay_event);
    void merge_by_sharevariables(cluster_t *cluster, BreakpointTrapEvent *hwbr_event);
    void merge_by_rlworks(cluster_t *cluster, RunLoopBoundaryEvent *rlboundary_event);
    void merge_by_mkrunnable(cluster_t *cur_cluster, MakeRunEvent *wakeup_event);
    void merge_by_waitsync(cluster_t *cluster, WaitEvent *wait_event);
    void add_waitsync(cluster_t *cluster);

public:
    ClusterGen(Groups *groups_ptr, bool connect_mkrun);
    ~ClusterGen(void);

    cluster_t *cluster_of(Group *);
    uint64_t get_size(void) { return clusters.size();}
    std::map<uint64_t, cluster_t *> &get_clusters() {return clusters;}
    Groups *get_groups_ptr() {return groups_ptr;}

    void streamout_clusters(std::string &output_path);
    void js_clusters(std::string &output_path);

    void inspect_clusters(std::string &output_path);
    void compare(ClusterGen *peer, std::string &output_path);
    void check_connection(uint64_t gid_1, uint64_t gid_2, std::string &output_path);
    void identify_similar_clusters(std::string &output_path);
};
typedef ClusterGen clusters_t;

#endif
