#include "cluster_filter.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

Filter::Filter(clusters_t *_clusters_ptr)
{
    clusters_ptr = _clusters_ptr;
    filtered_clusters.clear();
    process_set.clear();
    select_clusters(clusters_ptr);
    path_filter = process_filter = false;
}

Filter::~Filter()
{
    std::map<uint64_t, cluster_t *>::iterator it;
    for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
        if (it->second)
            delete(it->second);
    }
}

void Filter::select_clusters(clusters_t *clusters_ptr)
{
    Groups *groups_ptr = clusters_ptr->get_groups_ptr();
    std::map<uint64_t, Group *> &host_groups = groups_ptr->get_host_groups();
    std::map<uint64_t, Group *>::iterator it;

    original_clusters.clear();
    for (it = host_groups.begin(); it != host_groups.end(); it++) {
        cluster_t *host_cluster = clusters_ptr->cluster_of(it->second);
        if (host_cluster)
            original_clusters[host_cluster->get_cluster_id()] = host_cluster;
    }
}

int Filter::back_traverse_from(Group *colored_group, bool *visited, cluster_t *cur_cluster,
                node_edges_map_t &to_edges, cluster_t *dst_cluster)
{
    assert(visited && colored_group);
    int idx = cur_cluster->get_node_idx_in_cluster(colored_group);
    assert(idx >= 0);
    if (visited[idx] == true || idx == 0)
        return 0;
    visited[idx] = true;

    multimap_range_t range = to_edges.equal_range(colored_group);
    if (range.first == range.second)
        return 0;

    //std::cerr << "edge size = " << distance(range.first, range.second) << std::endl;
    node_edges_map_t::iterator it;
    rel_t edge;
    Group *g_from;

    for (it = range.first; it != range.second; it++) {
        edge = it->second;
        dst_cluster->add_edge(edge.g_from, edge.g_to, edge.e_from, edge.e_to, edge.rel_type);
        g_from = edge.g_from;
        if (g_from == colored_group)
            continue;
        if (dst_cluster->get_node_idx_in_cluster(g_from) != -1)
            continue;
        //std::cerr << "Traverse through node " << std::hex << g_from->get_group_id() << " from Cluster " << std::hex << cur_cluster->get_cluster_id() << std::endl;
        dst_cluster->add_node(g_from);
        back_traverse_from(g_from, visited, cur_cluster, to_edges, dst_cluster);
    }
    return 0;
}

cluster_t *Filter::filter_by_traverse(int index, cluster_t *cur_cluster)
{
    Groups *groups_ptr = clusters_ptr->get_groups_ptr();
    std::map<uint64_t, Group *> &host_groups = groups_ptr->get_host_groups();
    node_edges_map_t to_edges, from_edges;
    std::vector<Group*> groups = cur_cluster->get_nodes();
    bool *visited = (bool *)malloc(sizeof(bool) * groups.size());

    if (visited == nullptr) {
        std::cerr << "Error : OOM" << std::endl;
        return nullptr;
    }
    memset(visited, 0, sizeof(bool) * groups.size());

    cur_cluster->classify_cluster_edges(to_edges, from_edges);

    cluster_t *dst_cluster = new Cluster();
    if (dst_cluster == nullptr) {
        std::cerr << "Error : OOM" << std::endl;
        return nullptr;
    }
    dst_cluster->set_cluster_id(cur_cluster->get_cluster_id());
    
    std::vector<Group*>::iterator it;
    Group *colored_group;
    for (it = groups.begin(); it != groups.end(); it++) {
        if (host_groups.find((*it)->get_group_id()) == host_groups.end()
            || dst_cluster->get_node_idx_in_cluster(*it) != -1)
            continue;
        colored_group = *it;
        //std::cerr << "Add node " << std::hex << colored_group->get_group_id() << " from Cluster " << std::hex << cur_cluster->get_cluster_id() << std::endl;
        dst_cluster->add_node(colored_group);
         back_traverse_from(colored_group, visited, cur_cluster, to_edges, dst_cluster);
    }
    //TODO : check if it misses edges!!
    free(visited);
    filtered_clusters[index] = dst_cluster;
    return dst_cluster;
}

bool Filter::read_processes(std::string filename)
{
    std::ifstream input(filename);
    if (input.fail()) {
        std::cerr << "No file found for process intersection" << std::endl;
        return false;
    }

    std::string line;
    while (getline(input, line) && line.size() > 0) {
        process_set.insert(line);
    }
    //std::cerr << "Intersection size = " << process_set.size() << std::endl;
    input.close();
    if (process_set.size())
        return true;
    return false;
}

bool Filter::erase_edge_in_range(rel_t edge, multimap_range_t &range, node_edges_map_t &edges)
{
    int range_distance = distance(range.first, range.second);
    if (range_distance == 0)
        return false;
        
    multistd::map<Group *, rel_t>::iterator edge_it;
    for (edge_it = range.first; edge_it != range.second; edge_it++) {
        if (edge_it->second == edge) {
            edges.erase(edge_it);
            return true;
        }
    }
    return false;
}

bool Filter::audit_deletion_of_node(Group *cur_node, node_edges_map_t &edges)
{
    multimap_range_t range = edges.equal_range(cur_node);
    if (distance(range.first, range.second) == 0)
        return true;
    return false;
}

void Filter::remove_node(cluster_t *cur_cluster, Group *cur_node, node_edges_map_t &to_edges, node_edges_map_t &from_edges)
{
    std::vector<Group *> propagate_deletion_nodes;
    propagate_deletion_nodes.clear();
    /* check every edge pointing to cur_node and delete it 
     * do update on from_edges_map and to_edges_map
     *   if the edge connected from node_B
     *       remove the particular edge in the node_B's from_edges_map
     * remove all the edges pointing to cur_node in to_edges_map
     */
    multimap_range_t to_range = to_edges.equal_range(cur_node);
    multistd::map<Group *, rel_t>::iterator edge_it;
    rel_t edge;
    for (edge_it = to_range.first; edge_it != to_range.second; edge_it++) {
        edge = edge_it->second;
        cur_cluster->remove_edge(edge);

        if (edge.g_from != edge.g_to) {
            auto peer_from_range = from_edges.equal_range(edge.g_from);
            erase_edge_in_range(edge, peer_from_range, from_edges);
            /* check if need propagate the removing to the node edge.g_from
             * if neither from edges nor to edges with other nodes, delete it
             */
            if (audit_deletion_of_node(edge.g_from, from_edges)
                    && audit_deletion_of_node(edge.g_from, to_edges))
                propagate_deletion_nodes.push_back(edge.g_from);
        }
    }
    to_edges.erase(to_range.first, to_range.second);

    /* check every edge pointed from cur_node (node_A) and delete it 
     * do update on from_edges_map and to_edges_map
     *   if the edge connected to node_B
     *       remove the particular edge in the node_B's to_edges_map
     * remove all the edges from cur_node in from_edges_map
     */
    multimap_range_t from_range = from_edges.equal_range(cur_node);
    for (edge_it = from_range.first; edge_it != from_range.second; edge_it++) {
        rel_t edge = edge_it->second;
        cur_cluster->remove_edge(edge);

        if (edge.g_from != edge.g_to) {
            auto peer_to_range = to_edges.equal_range(edge.g_to);
            erase_edge_in_range(edge, peer_to_range, to_edges);
            /* check if need propagate the removing to the node edge.g_to
             * if there is not from edges any more with other nodes, delete it
             */
            if (audit_deletion_of_node(edge.g_to, to_edges))
                propagate_deletion_nodes.push_back(edge.g_to);
        }
    }
    from_edges.erase(from_range.first, from_range.second);

    //remove current node
    std::cerr <<  "Delete " << std::hex << cur_node->get_group_id() << " with size = " << std::hex <<  cur_node->get_size() << std::endl;
    cur_cluster->remove_node(cur_node);

    std::vector<Group *> ::iterator node_it;
    for (node_it = propagate_deletion_nodes.begin();
            node_it != propagate_deletion_nodes.end(); node_it++) {
        //propagate the deletion of nodes
        remove_node(cur_cluster, *node_it, to_edges, from_edges);
    }
}

cluster_t *Filter::filter_by_proc_intersection(cluster_t *cluster)
{
    if (process_set.size() == 0)
        return cluster;

    node_edges_map_t to_edges, from_edges;
    cluster->classify_cluster_edges(to_edges, from_edges);
    std::vector<Group *> nodes = cluster->get_nodes();
    std::vector<Group *>::iterator it;

    for (it = nodes.begin(); it != nodes.end(); it++) {
        Group *cur_node =  *it;
        std::string proc_name(cur_node->get_first_event()->get_procname());
        if (process_set.find(proc_name) != process_set.end())
            continue;
        // remove from the node that is not in the process set
        remove_node(cluster, cur_node, to_edges, from_edges);
    }
    return cluster;
}

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
void Filter::clusters_filter_para(uint64_t type)
{
    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));

    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

    std::map<uint64_t, cluster_t *>::iterator it;

    switch (type) {
        case PATH_FILTER: {
            for (it = original_clusters.begin(); it != original_clusters.end(); it++)
                filtered_clusters[it->first] = nullptr;
            for (it = original_clusters.begin(); it != original_clusters.end(); it++) {
                filter_by_traverse(it->first, it->second);
                //ioService.post(boost::bind(&Filter::filter_by_traverse, this, it->first, it->second));
                //OOM
            }
            path_filter = true;
            break;
        }
        case PROCESS_FILTER: {
            if (read_processes("process_intersection.log") == false)
                break;

            if (path_filter) {
                for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++)
                    if (it->second != nullptr) {
                        ioService.post(boost::bind(&Filter::filter_by_proc_intersection, this, it->second));
                    }
            } else {
                for (it = original_clusters.begin(); it != original_clusters.end(); it++)
                    ioService.post(boost::bind(&Filter::filter_by_proc_intersection, this, it->second));
            }
            process_filter = true;
            break;
        }
        default:
            break;
    }
    work.reset();
    threadpool.join_all();
    std::cerr << "Finished Filtering with type: ";
    if (type == PATH_FILTER)
        std::cerr << "PATH TRAVERSE" << std::endl;
    else if (type == PROCESS_FILTER)
        std::cerr << "PROCESS INTERSECTION" << std::endl;
}

void Filter::clusters_filter(uint64_t types)
{
    if (types & PATH_FILTER)
        clusters_filter_para(PATH_FILTER);
    if (types & PROCESS_FILTER)
        clusters_filter_para(PROCESS_FILTER);
}

void Filter::streamout_filtered_clusters(std::string output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t *> &result_clusters = path_filter? filtered_clusters : original_clusters;
    std::map<uint64_t, cluster_t *>::iterator it;

    for (it = result_clusters.begin(); it != result_clusters.end(); it++) {
        cluster_t *cur_cluster = it->second;
        output << "#Cluster " << std::hex << cur_cluster->get_cluster_id();
        output << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
        cur_cluster->streamout_cluster(output);
    }
    output.close();
}

void Filter::decode_filtered_clusters(std::string output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t *> &result_clusters = path_filter? filtered_clusters : original_clusters;
    std::map<uint64_t, cluster_t *>::iterator it;

    for (it = result_clusters.begin(); it != result_clusters.end(); it++) {
        cluster_t *cur_cluster = it->second;
        output << "Cluster_" << cur_cluster->get_cluster_id();
        output << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
        cur_cluster->streamout_cluster(output);
    }
}

void Filter::decode_edges(std::string output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t *> &result_clusters = path_filter? filtered_clusters : original_clusters;
    std::map<uint64_t, cluster_t *>::iterator it;

    for (it = result_clusters.begin(); it != result_clusters.end(); it++) {
        cluster_t *cur_cluster = it->second;
        output << "Cluster_" << cur_cluster->get_cluster_id();
        output << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
        cur_cluster->decode_edges(output);
    }
}
