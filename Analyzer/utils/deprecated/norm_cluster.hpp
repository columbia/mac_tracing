//////////////////////////////////////////////
#ifndef NORM_CLUSTER_HPP
#define NORM_CLUSTER_HPP

#include "cluster.hpp"
#include "canonization.hpp"

class NormCluster {
    cluster_t *cluster;
    std::map<EventType::event_type_t, bool> &key_events;
    std::vector<NormGroup *> sort_vertexes;
    std::map<Group *, NormGroup *> vertexes_map;
    std::map<NormGroup *, Group *> vertexes_reverse_map;

    void init_vertex_degrees(std::vector<rel_t> &edges);
    void create_vertexes(std::vector<Group *> &nodes);

    static bool compare_vertexes_in(NormGroup *elem1, NormGroup *elem2);
    NormGroup *sort_vertexes_in(std::vector<NormGroup *>::iterator begin, std::vector<NormGroup *>::iterator end);
    void remove_degree_edges_from(NormGroup *group, node_edges_map_t &group_inedges_map, node_edges_map_t &group_outedges_map);
    void virtualize_tid_for_groups(void);

public:
    NormCluster(cluster_t *c,std::map<EventType::event_type_t, bool> &key_events);
    ~NormCluster();

    uint64_t original_size() {return cluster->get_nodes().size();}
    uint64_t original_edges() {return cluster->get_edges().size();}
    uint64_t get_cluster_id() {return cluster->get_cluster_id();}

    void topological_sort();
    std::vector<NormGroup *> &get_sort_vertexes(void) {return sort_vertexes;}

    bool operator==(NormCluster &other);
    bool operator!=(NormCluster &other);
    void decode_cluster(std::ofstream &output);
};
typedef NormCluster NormGroupcluster_t;

///////////////////////////////////////////////
class Normalizer {
    std::map<uint64_t, cluster_t *> &clusters;
    /* pick key events for normalization */
    std::map<EventType::event_type_t, bool> key_events;
    std::map<uint64_t, NormGroupcluster_t *> NormGroupclusters;
    std::vector<NormGroupcluster_t *> outstanders;

    /*nomailization*/
    void normalize_cluster(uint64_t index, cluster_t *cluster);

public:
    Normalizer(std::map<uint64_t, cluster_t *> &clusters);
    ~Normalizer();
    void normalize_clusters(void);
    void sort_clusters(void);
    std::map<uint64_t, NormGroupcluster_t *> &get_normed_clusters(void) {return NormGroupclusters;}
    void compare(Normalizer* base, const char *outfile);
    void decode_outstand_cluster(const char *outfile);
};
typedef Normalizer normalizer_t;

#endif
