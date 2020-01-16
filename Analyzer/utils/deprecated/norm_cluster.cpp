#include "NormGroupcluster.hpp"

#define DEBUG_NORMCLUSTER 0

NormCluster::NormCluster(cluster_t *c,std::map<EventType::event_type_t, bool> &_key_events)
:key_events(_key_events)
{
    if (!(cluster = c))
        return;
    sort_vertexes.clear();
    vertexes_map.clear();
    create_vertexes(c->get_nodes());
    init_vertex_degrees(c->get_edges());
}

NormCluster::~NormCluster(void)
{
    vertexes_map.clear();
    vertexes_reverse_map.clear();
    std::vector<NormGroup *>::iterator it;
    for (it = sort_vertexes.begin(); it != sort_vertexes.end(); it++) {
        delete(*it);
    }
    sort_vertexes.clear();
    cluster = nullptr;
}

void NormCluster::init_vertex_degrees(std::vector<rel_t> &edges)
{
    std::vector<NormGroup *>::iterator ng_it;
    for (ng_it = sort_vertexes.begin(); ng_it != sort_vertexes.end(); ng_it++) {
        (*ng_it)->clear_degrees();
    }

    std::vector<rel_t>::iterator it;
    for (it = edges.begin(); it != edges.end(); it++) {
        rel_t edge = *it;
        if (edge.g_to == edge.g_from)
            continue;
        if (vertexes_map.find(edge.g_from) != vertexes_map.end())
            vertexes_map[edge.g_from]->add_out_edge();
        if (vertexes_map.find(edge.g_to) != vertexes_map.end())
            vertexes_map[edge.g_to]->add_in_edge();
    }
}

void NormCluster::create_vertexes(std::vector<Group *> &nodes)
{
    std::vector<Group *>::iterator it;
    Group *cur_group;
    NormGroup *norm_group;
    
    // normalize from group to group
    for (it = nodes.begin(); it != nodes.end(); it++) {
        assert(*it);
        cur_group = *it;
        norm_group = new NormGroup(cur_group, key_events);
        if (!norm_group) {
#if DEBUG_NORMCLUSTER 
            mtx.lock();
            std::cerr << "OOM, unable to allocate memory for normalizing group " << std::endl;
            mtx.unlock();
#endif
            exit(EXIT_FAILURE);
        }
        vertexes_map[cur_group] = norm_group;
        vertexes_reverse_map[norm_group] = cur_group;
        sort_vertexes.push_back(norm_group);
    }
}

bool NormCluster::compare_vertexes_in(NormGroup *elem1, NormGroup *elem2)
{
    return elem1->get_in_edges() != elem2->get_in_edges() ? elem1->get_in_edges() < elem2->get_in_edges() : \
                                    elem1->original_size() < elem2->original_size();
}

NormGroup *NormCluster::sort_vertexes_in(std::vector<NormGroup *>::iterator begin, std::vector<NormGroup *>::iterator end)
{
    assert(begin != end);
    sort(begin, end, &NormCluster::compare_vertexes_in);
#if DEBUG_NORMCLUSTER
    mtx.lock();
    std::cerr << "Least in Node is " << std::hex << (*begin)->get_group_id();
    std::cerr << "\tIn edges: " << std::dec << (*begin)->get_in_edges() << std::endl;
    std::vector<NormGroup *>::iterator it = begin;
    for (; it != end; it++)
        std::cerr << std::dec << (*it)->get_in_edges() << "\t";
    std::cerr << std::endl;
    mtx.unlock();
#endif
    return *begin;
}

void NormCluster::remove_degree_edges_from(NormGroup *group, node_edges_map_t &group_inedges_map, node_edges_map_t &group_outedges_map)
{
    if (!group) {
#if DEBUG_NORMCLUSTER
        mtx.lock();
        std::cerr << "No group to remove" << std::endl;
        mtx.unlock();
#endif
        return;
    }
    node_edges_map_t::iterator it;
    auto range = group_outedges_map.equal_range(vertexes_reverse_map[group]);
#if DEBUG_NORMCLUSTER
    mtx.lock();
    std::cerr << "Remove in edges from " << distance(range.first, range.second) << "nodes"<< std::endl;
    mtx.unlock();
#endif

    for (it = range.first; it != range.second; it++) {
        rel_t edge = it->second;
        vertexes_map[edge.g_to]->dec_in_edge();
    }
}

void NormCluster::topological_sort(void)
{
    std::vector<NormGroup *>::iterator begin = sort_vertexes.begin(), end = sort_vertexes.end();
    node_edges_map_t group_inedges_map, group_outedges_map;
    cluster->classify_cluster_edges(group_inedges_map, group_outedges_map);
    while(begin != end) {
        remove_degree_edges_from(sort_vertexes_in(begin, end), group_inedges_map, group_outedges_map);
        begin++;
    }
    //restore node's degrees
    init_vertex_degrees(cluster->get_edges());
}

void NormCluster::virtualize_tid_for_groups(void)
{
    

}

bool NormCluster::operator==(NormCluster &other)
{
    std::vector<NormGroup *> other_vertexes = other.get_sort_vertexes();
    std::vector<NormGroup *>::iterator other_it, other_end = other_vertexes.end();
    std::vector<NormGroup *>::iterator this_it, this_end = sort_vertexes.end();
    if (other_vertexes.size() != sort_vertexes.size())
        return false;
    
    for (;this_it != this_end; this_it++, other_it++) {
        assert(other_it != other_end);
        if (*(*other_it) != *(*this_it))
            return false;
    }
    return true;
}

bool NormCluster::operator!=(NormCluster &other)
{
    return !(*this == other);
}

void NormCluster::decode_cluster(std::ofstream &output)
{
    std::vector<NormGroup *>::iterator it;
    for (it = sort_vertexes.begin(); it != sort_vertexes.end(); it++) {
        (*it)->decode_group(output);
    }
}
