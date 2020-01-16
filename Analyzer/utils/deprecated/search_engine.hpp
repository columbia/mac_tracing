#ifndef BUG_SEARCH_HPP
#define BUG_SEARCH_HPP
#include "cluster.hpp"
#include "canonization.hpp"

typedef uint64_t thread_id_t;
typedef uint64_t group_id_t;

class BugSearcher {
    std::map<uint64_t, cluster_t *> cluster_map;
    EventBase *ceiling_event;
    EventBase *floor_event;
    EventBase *search_source_event_in_main_thread(int event_type, EventBase *event);
    cluster_t *search_cluster_overlap(EventBase *event);

    ////////////////////////////////////////
    Groups *groups_ptr;
    std::map<thread_id_t,std::map<group_id_t, NormGroup*> > normalized_groups_map;
    std::map<EventType::event_type_t, bool> add_key_events();

public:
    BugSearcher(clusters_t *cluster_gen);
    BugSearcher(Groups *groups);
    ~BugSearcher() {cluster_map.clear(); groups_ptr = nullptr; ceiling_event = floor_event = nullptr;};

    cluster_t *report_spinning_cluster();
    template <typename T>
    T max_item_key(std::map<T, double> &maps, T comb);
    //////////////////////////////////////////////
    //
    WaitEvent *suspicious_blocking(cluster_t *cluster);
    std::map<WaitEvent*, double> suspicious_blocking(Groups *groups, std::string outfile);
    Group *suspicious_segment(cluster_t *cluster);
    //
    std::vector<uint64_t> get_counterpart(uint64_t group_id, thread_id_t thread);
    void update_normalized_map_for_thread(thread_id_t thread);
    void slice_path(uint64_t group_id, std::string outfile);
};
#endif

