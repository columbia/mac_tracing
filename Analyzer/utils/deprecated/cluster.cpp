#include "cluster.hpp"
#include <algorithm>

#define DEBUG_CONN_TRAVERSE 1

Cluster::Cluster(void)
{
    cluster_id = -1;
    edges.clear();
    nodes.clear();
}

Cluster::Cluster(Group *group)
{
    root = group;
    cluster_id = group->get_group_id();
    edges.clear();
    nodes.clear();
    add_node(group);
    connectors.clear();
}

Cluster::~Cluster(void)
{
    edges.clear();
    nodes.clear();
}

void Cluster::push_connectors(Group *group, EventBase *exclude)
{
    std::list<EventBase *> events = group->get_container();
    std::list<EventBase *>::iterator it;
    EventBase *event;
    for (it = events.begin(); it != events.end(); it++) {
        event = *it;
        if (event == exclude)
            continue;

        switch (event->get_event_type()) {
            case MSG_EVENT:
                connectors.push_back(dynamic_cast<MsgEvent *>(event));
                break;
            case DISP_ENQ_EVENT:
                connectors.push_back(dynamic_cast<BlockEnqueueEvent *>(event));
                break;
            case DISP_DEQ_EVENT:
                connectors.push_back(dynamic_cast<BlockDequeueEvent *>(event));
                break;
            case TMCALL_CREATE_EVENT:
                connectors.push_back(dynamic_cast<TimerCreateEvent *>(event));
                break;
            case CA_SET_EVENT:
                connectors.push_back(dynamic_cast<CASetEvent *>(event));
                break;
            case BREAKPOINT_TRAP_EVENT:
                connectors.push_back(dynamic_cast<BreakpointTrapEvent *>(event));
                break;
            case RL_BOUNDARY_EVENT: {
                RunLoopBoundaryEvent *rlboundary_event = dynamic_cast<RunLoopBoundaryEvent *>(event);
                if (rlboundary_event->get_owner() != nullptr)
                    connectors.push_back(rlboundary_event);
                break;
            }
            case MR_EVENT:
                connectors.push_back(dynamic_cast<MakeRunEvent *>(event));
                break;
            case WAIT_EVENT:
                wait_events.push_back(dynamic_cast<WaitEvent *>(event));
                break;
            default:
                break;
        }
    }
}

std::list<EventBase *> Cluster::pop_cur_connectors()
{
    std::list<EventBase *> ret = connectors;
    connectors.clear();
    return ret;
}

std::list<WaitEvent *> &Cluster::get_wait_events()
{
    return wait_events;
}

void Cluster::add_edge(Group *g1, Group *g2, EventBase *t1, EventBase *t2, uint32_t type)
{
    struct rel_t edge = {g1, g2, t1, t2, type};

    std::vector<rel_t>::iterator it = find_if(edges.begin(), edges.end(), 
        [&target_edge = edge]
        (const rel_t &m) -> bool{return target_edge == m;});

    if (it == edges.end())
        edges.push_back(edge);
}

bool Cluster::add_node(Group *g)
{
    if (find(nodes.begin(), nodes.end(), g) != nodes.end())
        return false;
    
    g->set_cluster_idx(cluster_id);
    nodes.push_back(g);
    return true;
}

bool Cluster::remove_edge(const rel_t edge)
{
    std::vector<rel_t>::iterator it = find_if(edges.begin(), edges.end(), 
        [&target_edge = edge]
        (const rel_t &m) -> bool{return target_edge == m;});

    if (it != edges.end()) {
        edges.erase(it);
        return true;
    }

    return false;
}

bool Cluster::remove_node(Group *node)
{
    std::vector<Group *>::iterator it = find(nodes.begin(), nodes.end(), node);
    if (it != nodes.end()) {
        nodes.erase(it);
        return true;
    }
    return false;
}

void Cluster::append_nodes(std::vector<Group *> &n)
{
    std::vector<Group *>::iterator it;
    for (it = n.begin(); it != n.end(); it++) {
        (*it)->set_cluster_idx(cluster_id);
    }
    nodes.insert(nodes.end(), n.begin(), n.end());
}

void Cluster::append_edges(std::vector<rel_t> &g)
{
    edges.insert(edges.end(), g.begin(), g.end());
}

bool Cluster::compare_time(Group *node1, Group *node2)
{
    double timestamp1 = node1->get_container().front()->get_abstime();
    double timestamp2 = node2->get_container().front()->get_abstime();

    if (timestamp1 - timestamp2 <= 10e-8 && timestamp1-timestamp2 >= -10e-8)
        return (node1->get_container().front()->get_tid() \
            < node2->get_container().front()->get_tid());
    return (timestamp1 < timestamp2);
}

void Cluster::sort_nodes()
{
    sort(nodes.begin(), nodes.end(), &Cluster::compare_time);
}

bool Cluster::compare_edge_from(rel_t edge1, rel_t edge2)
{
    if (edge1.g_from->get_group_id() != edge2.g_from->get_group_id())
        return edge1.g_from->get_group_id() < edge2.g_from->get_group_id();
    return compare_time(edge1.g_from, edge2.g_from);
}

void Cluster::sort_edges()
{
    sort(edges.begin(), edges.end(), &Cluster::compare_edge_from);
}

/* ignore edges to itself */
void Cluster::classify_cluster_edges(node_edges_map_t &to_edges, node_edges_map_t &from_edges)
{
    std::vector<rel_t>::iterator it;
    to_edges.clear();
    from_edges.clear();
    for (it = edges.begin(); it != edges.end(); it++) {
        rel_t edge = *it;
        if (edge.g_to == edge.g_from)
            continue;
        to_edges.insert(std::make_pair(edge.g_to, edge));
        from_edges.insert(std::make_pair(edge.g_from, edge));
    }
}

rel_t Cluster::direct_edge(std::string from_proc, std::string to_proc)
{
    std::vector<rel_t>::iterator it;
    for (it = edges.begin(); it != edges.end(); it++) {
        rel_t edge = *it;
        if ((edge.e_from->get_procname() == from_proc && edge.e_to->get_procname() == to_proc)
            || (edge.e_from->get_procname() == to_proc && edge.e_to->get_procname() == from_proc))
            return edge; 
    }
    rel_t empty = {nullptr, nullptr, nullptr, nullptr, 0};
    return empty;
}

int Cluster::get_node_idx_in_cluster(Group *node)
{
    std::vector<Group *>::iterator pos = find(nodes.begin(), nodes.end(), node);
    int ret = -1;
    if (pos != nodes.end())
        ret = pos - nodes.begin();
    return ret;
}

bool Cluster::traverse(std::string to_proc, Group *cur_node, bool *visited,
        node_edges_map_t &from_edges, node_edges_map_t &to_edges, std::vector<rel_t> &paths,
        std::map<std::pair<Group *, std::string>, std::vector<rel_t> > &sub_result)
{
    multimap_range_t from_range = from_edges.equal_range(cur_node);
    multimap_range_t to_range = to_edges.equal_range(cur_node);

    if (from_range.first == from_range.second && to_range.first == to_range.second) 
        return false;

    std::pair<Group *, std::string> key = std::make_pair(cur_node, to_proc);
    if (sub_result.find(key) != sub_result.end()) {
        std::vector<rel_t> remaining = sub_result[key];
        paths.insert(paths.end(), remaining.begin(), remaining.end());
        return true;
    }

    visited[get_node_idx_in_cluster(cur_node)] = true;

    node_edges_map_t::iterator it;
    Group *peer_node;
    std::string peer_proc;

    for (it = from_range.first; it != from_range.second; it++) {
        peer_node = it->second.g_to;
        peer_proc = it->second.e_to->get_procname();
        if (visited[get_node_idx_in_cluster(peer_node)] == true)
            continue;
        paths.push_back(it->second);
        //if (it->second.e_to->get_procname() == to_proc
        if (peer_proc == to_proc || traverse(to_proc, peer_node, visited, from_edges, to_edges, paths, sub_result) == true) {
            std::pair<Group *, std::string> key = std::make_pair(peer_node, to_proc);
            sub_result[key] = paths;
            return true;
        }
        paths.pop_back();
    }

    for (it = to_range.first; it != to_range.second; it++) {
        peer_node = it->second.g_from;
        peer_proc = it->second.e_from->get_procname();
        if (visited[get_node_idx_in_cluster(peer_node)] == true)
            continue;
        paths.push_back(it->second);
        //if (it->second.e_from->get_procname() == to_proc
        if (peer_proc == to_proc || traverse(to_proc, peer_node, visited, from_edges, to_edges, paths, sub_result) == true) {
            std::pair<Group *, std::string> key = std::make_pair(peer_node, to_proc);
            sub_result[key] = paths;
            return true;
        }
        paths.pop_back();
    }

    visited[get_node_idx_in_cluster(cur_node)] = false;
    return false;
}

void Cluster::search_paths(std::string from_proc, std::string to_proc, std::ofstream &outfile,
        std::map<std::pair<Group *, std::string>, std::vector<rel_t> > &sub_result)
{
    std::set<Group *> from_nodes;
    node_edges_map_t to_edges, from_edges;
    std::vector<Group *>::iterator it;

    for (it = nodes.begin(); it != nodes.end(); it++)
        if ((*it)->get_first_event()->get_procname() == from_proc)
            from_nodes.insert(*it);

    classify_cluster_edges(to_edges, from_edges);

    //search inside the cluster and store the paths
    bool *visited = (bool *)malloc(sizeof(bool) * nodes.size());
    std::vector<rel_t> paths;
    if (visited == nullptr) {
        mtx.lock();
        std::cerr << "Error: OOM " << __func__ << std::endl;
        mtx.unlock();
        return;
    }
    std::set<Group *>::iterator node_it;
    for (node_it = from_nodes.begin(); node_it != from_nodes.end(); node_it++) {
        memset(visited, 0, nodes.size());
        paths.clear();
        if (traverse(to_proc, *node_it, visited, from_edges, to_edges, paths, sub_result)) {
            //flush_out_result    
            outfile << "Path from " << from_proc << " to " << to_proc << std::endl;
            std::vector<rel_t>::iterator edge_it;
            for (edge_it = paths.begin(); edge_it != paths.end(); edge_it ++) {
                //outfile << "\trel " << (*edge_it).e_from->get_procname();
                //outfile << " " << std::right << std::hex << (*edge_it).g_from->get_group_id();
                //outfile << " at " << std::fixed << std::setprecision(1) << (*edge_it).e_from->get_abstime();
                //outfile << " -> " << (*edge_it).e_to->get_procname();
                //outfile << " " << std::right << std::hex << (*edge_it).g_to->get_group_id();
                //outfile << " at " << std::fixed << std::setprecision(1) << (*edge_it).e_to->get_abstime();
                //outfile << std::endl;
                decode_edge(outfile, *edge_it);
            }
            outfile << std::endl;

            std::pair<Group *, std::string> key = std::make_pair(*node_it, to_proc);
            sub_result[key] = paths;
            break;
        }
    }
    free(visited);
}

void Cluster::inspect_procs_irrelevance(std::ofstream &outfile)
{
    std::set<std::string> procs;
    std::set<std::string>::iterator proc_it, proc_jt;
    std::vector<Group *>::iterator node_it;
    std::map<std::pair<Group *, std::string>, std::vector<rel_t> > sub_result;

    for (node_it = nodes.begin(); node_it != nodes.end(); node_it++)
        procs.insert((*node_it)->get_first_event()->get_procname());
    
    for (proc_it = procs.begin(); proc_it != procs.end(); proc_it++)
        for (proc_jt = proc_it, proc_jt++; proc_jt != procs.end(); proc_jt++) {
            outfile << "Inspect connection between " << *proc_it << " and " << *proc_jt << std::endl; 
            rel_t edge = direct_edge(*proc_it, *proc_jt);
            if (edge.g_from != nullptr) {
                outfile << "Direct edge from " << *proc_it << " to " << *proc_jt << std::endl;
                std::pair<Group *, std::string> key1 = std::make_pair(edge.g_from, edge.e_to->get_procname());
                std::pair<Group *, std::string> key2 = std::make_pair(edge.g_to, edge.e_from->get_procname());
                std::vector<rel_t> paths;
                paths.clear();
                paths.push_back(edge);
                sub_result[key1] = paths;
                sub_result[key2] = paths;
            } else {
                search_paths(*proc_it, *proc_jt, outfile, sub_result);
            }
        }
}

void Cluster::compare(cluster_t *peer, std::ofstream &output)
{
    std::vector<rel_t> peer_edges = peer->get_edges();
    std::vector<rel_t>::iterator edge_it1, edge_it2;
    std::vector<rel_t> miss_edges, peer_matched_edges;
    output << "Compare cluster " << std::hex << cluster_id << " and " << std::hex << peer->get_cluster_id() << std::endl;
    
    for (edge_it1 = edges.begin(); edge_it1 != edges.end(); edge_it1++) {
        rel_t edge1 = *edge_it1;
        bool found = false;
        for (edge_it2 = peer_edges.begin(); edge_it2 != peer_edges.end(); edge_it2++) {
            if (edge1 == *edge_it2) {
                peer_matched_edges.push_back(*edge_it2);
                found = true;
            }
        }
        if (found == false)
            miss_edges.push_back(edge1);
    }

    std::vector<Group *> peer_nodes = peer->get_nodes();
    std::vector<Group *>::iterator node_it1, node_it2;
    std::vector<uint64_t> miss_gid, peer_matched_gid;
    for (node_it1 = nodes.begin(); node_it1 != nodes.end(); node_it1++) {
        bool found = false;
        for(node_it2 = peer_nodes.begin(); node_it2 != peer_nodes.end(); node_it2++) {
            if ((*node_it1)->get_group_id() == (*node_it2)->get_group_id()) {
                peer_matched_gid.push_back((*node_it2)->get_group_id());
                found = true;
            }
        }
        if (found == false)
            miss_gid.push_back((*node_it1)->get_group_id());
    }
    
    
    output << "< Nodes only in cluster " << std::hex << cluster_id << ":";
    int i = 0;
    for (std::vector<uint64_t>::iterator it = miss_gid.begin(); it != miss_gid.end(); it++) {
        if (i % 8 == 0)
            output << std::endl;
        output << std::hex << *it << "\t";
        i++;
    }
    output << std::endl;

    output << "< Edges only in cluster " << std::hex << cluster_id << ":" << std::endl;
    for (edge_it1 = miss_edges.begin(); edge_it1 != miss_edges.end(); edge_it1++)
        decode_edge(output, *edge_it1);

    output << "> Nodes only in cluster " << std::hex << peer->get_cluster_id() << ":";
    i = 0;
    for (node_it2 = peer_nodes.begin(); node_it2 != peer_nodes.end(); node_it2++) {
        std::vector<uint64_t>::iterator it = find(peer_matched_gid.begin(), peer_matched_gid.end(),
                (*node_it2)->get_group_id());
        if (it == peer_matched_gid.end()) {
            if (i % 8 == 0)
                output <<  std::endl;
            output << std::hex << (*node_it2)->get_group_id() << "\t";
            i++;
        }
    }
    output << std::endl;

    output << "> Edges only in cluster " << std::hex << peer->get_cluster_id() << ":" << std::endl;
    for (edge_it2 = peer_edges.begin(); edge_it2 != peer_edges.end(); edge_it2++) {
        rel_t edge  = *edge_it2;
        std::vector<rel_t>::iterator edge_it = find_if(peer_matched_edges.begin(), peer_matched_edges.end(), 
                [&target_edge = edge]
                (const rel_t &m) -> bool{return target_edge == m;});
        if (edge_it == peer_matched_edges.end())
            decode_edge(output, edge);
    }
}

bool Cluster::traverse_connection(Group *cur_node, Group *destination_node, bool *visited,
        node_edges_map_t &to_edges, node_edges_map_t &from_edges, std::vector<rel_t> &paths)
{
    multimap_range_t from_range = from_edges.equal_range(cur_node);
    multimap_range_t to_range = to_edges.equal_range(cur_node);
    if (from_range.first == from_range.second && to_range.first == to_range.second) 
        return false;

    node_edges_map_t::iterator it;
    for (it = from_range.first; it != from_range.second; it++) {
        Group *peer_node = (it->second).g_to;
#if DEBUG_CONN_TRAVERSE 
        std::cerr << "check connection: " << std::hex << cur_node->get_group_id() << " and " << std::hex << peer_node->get_group_id() << std::endl;
#endif
        if (visited[get_node_idx_in_cluster(peer_node)] == true)
            continue;

        if (peer_node == destination_node) {
            visited[get_node_idx_in_cluster(peer_node)] = true;
            paths.push_back(it->second);
            return true;
        }

        visited[get_node_idx_in_cluster(peer_node)] = true;
        if (traverse_connection(peer_node, destination_node, visited, to_edges, from_edges, paths)) {
            paths.push_back(it->second);
            return true;
        }
        
    }
    
    for (it = to_range.first; it != to_range.second; it++) {
        Group *peer_node = (it->second).g_from;
#if DEBUG_CONN_TRAVERSE 
        std::cerr << "check connection: " << std::hex << cur_node->get_group_id() << " and " << std::hex << peer_node->get_group_id() << std::endl;
#endif
        if (visited[get_node_idx_in_cluster(peer_node)] == true)
            continue;

        if (peer_node == destination_node) {
            visited[get_node_idx_in_cluster(peer_node)] = true;
            paths.push_back(it->second);
            return true;
        }
        visited[get_node_idx_in_cluster(peer_node)] = true;
        if (traverse_connection(peer_node, destination_node, visited, to_edges, from_edges, paths)) {
            paths.push_back(it->second);
            return true;
        }
    }

    return false;
}

void Cluster::check_connection(Group *group_1, Group *group_2, std::ofstream &outfile)
{
    node_edges_map_t to_edges, from_edges;
    classify_cluster_edges(to_edges, from_edges);

    //search inside the cluster and store the paths
    bool *visited = (bool *)malloc(sizeof(bool) * nodes.size());
    if (visited == nullptr) {
        mtx.lock();
        std::cerr << "Error: OOM " << __func__ << std::endl;
        mtx.unlock();
        return;
    }
    memset(visited, 0, nodes.size());

    std::vector<rel_t> paths;
    paths.clear();
    visited[get_node_idx_in_cluster(group_1)] = true;

    if (traverse_connection(group_1, group_2, visited, to_edges, from_edges, paths)) {
        //write paths to output
        outfile << "Path from " << std::hex << group_1->get_group_id() << " to " << std::hex << group_2->get_group_id() << std::endl;
        std::vector<rel_t>::iterator edge_it;
        for (edge_it = paths.begin(); edge_it != paths.end(); edge_it ++) {
            //outfile << "\trel " << (*edge_it).e_from->get_procname();
            //outfile << " " << std::right << std::hex << (*edge_it).g_from->get_group_id();
            //outfile << " at " << std::fixed << std::setprecision(1) << (*edge_it).e_from->get_abstime();
            //outfile << " -> " << (*edge_it).e_to->get_procname();
            //outfile << " " << std::right << std::hex << (*edge_it).g_to->get_group_id();
            //outfile << " at " << std::fixed << std::setprecision(1) << (*edge_it).e_to->get_abstime();
            //outfile << std::endl;
            decode_edge(outfile, *edge_it);
        }
        outfile << std::endl;
    } else {
        outfile << "No connection get recognized !!!" << std::endl;
    }
}

void Cluster::js_lanes(std::ofstream &outfile)
{
    std::set<std::string> lanes;
    std::vector<Group *>::iterator it;
    EventBase * first_event = nullptr;

    for (it = nodes.begin(); it != nodes.end(); it++) {
        first_event = (*it)->get_first_event();
        std::stringstream ss;
        ss << std::hex << first_event->get_tid();
        lanes.insert(first_event->get_procname() + "_" + ss.str());
    }

    std::set<std::string>::iterator lane_it;
    outfile << "var lanes = [" << std::endl;
    for (lane_it = lanes.begin(); lane_it != lanes.end(); lane_it++)
        outfile << "\"" << *lane_it << "\"," << std::endl;
    outfile << "];" << std::endl;
}

void Cluster::js_groups(std::ofstream &outfile)
{
    std::vector<Group *>::iterator it;
    EventBase * first_event = nullptr, * last_event = nullptr;
    bool comma = false;

    outfile << "var groups = [" << std::endl;
    for (it = nodes.begin(); it != nodes.end(); it++) {
        if (comma)
            outfile << "," << std::endl;
        else
            comma = true;
        first_event = (*it)->get_first_event();
        last_event = (*it)->get_last_event();
        uint64_t time_begin, time_end;
        outfile <<"{lane: \"";
        outfile << first_event->get_procname();
        outfile << "_" << std::hex << first_event->get_tid();
        outfile <<"\", start: ";
        time_begin = static_cast<uint64_t>(first_event->get_abstime() * 10);
        outfile << std::dec << time_begin << ", end: ";
        time_end = static_cast<uint64_t>(last_event->get_abstime() * 10);
        outfile << std::dec << time_end << ", duration: ";
        outfile << std::dec << time_end - time_begin << ", name: \"";
        outfile << std::hex << (*it)->get_group_id() << "\"}";
    }
    outfile << "];" << std::endl;
}

void Cluster::js_edge(std::ofstream &outfile, EventBase *host, const char *action, EventBase *peer, bool *comma)
{
    uint64_t time;
    peer = peer ? peer : host;
    if (*comma == true)
        outfile << "," << std::endl;
    else 
        *comma = true;

    outfile << "{laneFrom: \"" << host->get_procname();
    outfile << "_" << std::hex << host->get_tid() << "\",";
    outfile << "laneTo: \"" << peer->get_procname();
    outfile << "_" << std::hex << peer->get_tid() << "\",";
    time = static_cast<uint64_t>(host->get_abstime() * 10);
    outfile << "timeFrom: " << std::dec << time << ",";
    time = static_cast<uint64_t>(peer->get_abstime() * 10);
    outfile << "timeTo: " << std::dec << time << ",";
    outfile << "label:\"" << action << "\"}";
}

void Cluster::message_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    MsgEvent *mach_msg_event = dynamic_cast<MsgEvent *>(event);
    EventBase *peer = nullptr;
    if (!mach_msg_event)
        return;
    if ((peer = mach_msg_event->get_peer())) {
        if (mach_msg_event->get_abstime() < peer->get_abstime())
            js_edge(outfile, mach_msg_event, "send", peer, comma);
        else
            js_edge(outfile, mach_msg_event, "recv", nullptr, comma);
    }

    if ((peer = mach_msg_event->get_next()))
        js_edge(outfile, mach_msg_event, "pass_msg", peer, comma);
}

void Cluster::dispatch_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    EventBase *peer = nullptr;
    BlockEnqueueEvent *enqueue_event = dynamic_cast<BlockEnqueueEvent *>(event);
    if (enqueue_event && ((peer = enqueue_event->get_consumer()))) {
        js_edge(outfile, enqueue_event, "dispatch", peer, comma);
        return;
    }

    BlockDequeueEvent *deq_event = dynamic_cast<BlockDequeueEvent *>(event);
    if (deq_event && ((peer = deq_event->get_invoke())))
        js_edge(outfile, deq_event, "deq_exec", peer, comma);
}

void Cluster::timercall_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    EventBase *peer = nullptr;
    TimerCreateEvent *timercreate_event = dynamic_cast<TimerCreateEvent *>(event);
    if (timercreate_event && ((peer = timercreate_event->get_called_peer())))
        js_edge(outfile, timercreate_event, "timer_callout", peer, comma);
}

void Cluster::sharevariable_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
}

void Cluster::runloop_item_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
}

void Cluster::mkrun_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    MakeRunEvent *mr_event = dynamic_cast<MakeRunEvent *>(event);
    EventBase *peer = nullptr;

    if (mr_event && (peer = mr_event->get_peer_event())) {
        std::string wakeup_resource("wakeup_");
        if (mr_event->get_wait() != nullptr)
            wakeup_resource.append(mr_event->get_wait()->get_wait_resource());
        js_edge(outfile, mr_event, wakeup_resource.c_str(), peer, comma);
    }
}

void Cluster::wait_label(std::ofstream &outfile, EventBase *event, bool *comma)
{
    WaitEvent *wait_event = dynamic_cast<WaitEvent *>(event);
    if (!wait_event)
        return;
    std::string wait_resource("wait_");
    wait_resource.append(wait_event->get_wait_resource());
    js_edge(outfile, wait_event, wait_resource.c_str(), nullptr, comma);
}

void Cluster::coreannimation_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    CASetEvent *caset_event = dynamic_cast<CASetEvent *>(event);
    EventBase *peer = nullptr;
    if (caset_event && (peer = caset_event->get_display_object()))
        js_edge(outfile, caset_event, "CoreAnimationUpdate", peer, comma);
}

void Cluster::js_arrows(std::ofstream &outfile)
{
    std::vector<Group *>::iterator it;
    std::list<EventBase *>::iterator event_it;
    bool comma = false;

    outfile <<"var arrows = [" << std::endl;
    for (it = nodes.begin(); it != nodes.end(); it++) {
        std::list<EventBase *> container = (*it)->get_container();
        if (container.size() == 0)
            continue;

        for (event_it = container.begin(); event_it != container.end(); event_it++) {
            switch((*event_it)->get_event_type()) {
                case MSG_EVENT:
                    message_edge(outfile, *event_it, &comma);
                    break;
                case DISP_ENQ_EVENT: 
                case DISP_DEQ_EVENT: 
                    dispatch_edge(outfile, *event_it, &comma);
                    break;
                case TMCALL_CREATE_EVENT:
                    timercall_edge(outfile, *event_it, &comma);
                    break;
                case MR_EVENT:
                    mkrun_edge(outfile, *event_it, &comma);
                    break;
                case WAIT_EVENT:
                    wait_label(outfile, *event_it, &comma);
                    break;
                case CA_SET_EVENT:
                    coreannimation_edge(outfile, *event_it, &comma);
                    break;
                case BREAKPOINT_TRAP_EVENT:
                    sharevariable_edge(outfile, *event_it, &comma);
                    break;
                case RL_BOUNDARY_EVENT:
                    runloop_item_edge(outfile, *event_it, &comma);
                    break;
                default:
                    break;
            }
        }
        //js_edge(outfile, last_event, "deactivate", nullptr);
    }
    outfile << "];" << std::endl;
}

void Cluster::js_cluster(std::ofstream &outfile)
{
    js_lanes(outfile);
    js_groups(outfile);
    js_arrows(outfile);
}

void Cluster::decode_edge(std::ofstream &outfile, rel_t edge)
{
    if (edge.g_from == edge.g_to)
        return;

    uint32_t type = edge.rel_type;
    EventBase *from_event = edge.e_from;
    EventBase *to_event = edge.e_to;

    outfile << std::hex << edge.g_from->get_group_id();
    outfile << "\t" << from_event->get_procname() << ", ";
    outfile << std::hex << edge.g_to->get_group_id();
    outfile << "\t" << to_event->get_procname() <<  ", ";

    switch (type) {
        case MSGP_REL: {
                   std::string msg1_type = from_event->get_op();
                   std::string msg2_type = to_event->get_op();
                   if (msg1_type == "ARGUS_IPC_msg_send" && msg2_type == "ARGUS_IPC_msg_recv")
                   outfile << "MACH_MSG_SEND";
                   else if (msg1_type == "ARGUS_IPC_msg_recv" && msg2_type == "ARGUS_IPC_msg_send")
                   outfile << "MACH_MSG_PASS";
                   else
                   outfile << "MACH_MSG_UNKNOWN";
                   break;
               }
        case MKRUN_REL:
               outfile << "MK_RUN";
               break;
        case DISP_EXE_REL:
               outfile << "DISPATCH_EXECUTE";
               break;
        case DISP_DEQ_REL:
               outfile << "DISPATCH_DEQUEUE";
               break;
        case CALLOUT_REL:
               outfile << "TIMER_CALLOUT"; 
               break;
        case CALLOUTCANCEL_REL:
               outfile << "CANCEL_CALLOUT";
               break;
        case CA_REL:
               outfile << "COREANNIMATION";
               break;
        case BRTRAP_REL:
               outfile << "HWBR_SHARED_VAR";
               break;
        case RLITEM_REL:
               outfile << "RUNLOOP_ITEM";
               break;
        default:
               outfile << "UNKNOWN " << type;
               break;
    }

    outfile << ", " << std::fixed << std::setprecision(1) << from_event->get_abstime();
    outfile << ", " << std::fixed << std::setprecision(1) << to_event->get_abstime();
    outfile << std::endl;
}

void Cluster::decode_edges(std::ofstream &outfile)
{
    std::vector<rel_t>::iterator edge_it;
    sort_edges();
    for (edge_it = edges.begin(); edge_it != edges.end(); edge_it++)
        decode_edge(outfile, *edge_it);

}

void Cluster::decode_cluster(std::ofstream &outfile)
{
    std::list<EventBase *> cluster_events;
    std::vector<Group *>::iterator it;

    for (it = nodes.begin(); it != nodes.end(); it++) {
        std::list<EventBase *> container = (*it)->get_container();
        cluster_events.insert(cluster_events.end(), container.begin(), container.end());
    }

    cluster_events.sort(Parse::EventComparator::compare_time);
    std::list<EventBase *>::iterator jt;
    for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++)
        (*jt)->decode_event(0, outfile);
}

void Cluster::streamout_cluster(std::ofstream &outfile)
{
    std::list<EventBase *> cluster_events;
    std::vector<Group *>::iterator it;
    
    for (it = nodes.begin(); it != nodes.end(); it++) {
        std::list<EventBase*> container = (*it)->get_container();
        cluster_events.insert(cluster_events.end(), container.begin(), container.end());
    }

    outfile << "#Events in cluster" << cluster_events.size() << std::endl;
    cluster_events.sort(Parse::EventComparator::compare_time);
    std::list<EventBase *>::iterator jt;
    for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++)
        (*jt)->streamout_event(outfile);
}
