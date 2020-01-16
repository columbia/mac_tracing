#include "parser.hpp"
#include "cluster.hpp"

typedef std::map<MakeRunEvent *, std::list<EventBase *>::iterator> mkrun_pos_t;

ClusterInit::ClusterInit(Groups *_groups_ptr)
:groups_ptr(_groups_ptr)
{
    clusters.clear();
    index = 0;
    assert(groups_ptr != nullptr);
    merge_by_mach_msg();
    merge_by_dispatch_ops();
    merge_by_mkrun();
    merge_by_timercallout();
    merge_by_ca();
    merge_by_breakpoint_trap();
}

ClusterInit::~ClusterInit(void)
{
    std::map<uint64_t, cluster_t *>::iterator it;
    for (it = clusters.begin(); it != clusters.end(); it++) {
        assert(it->second != nullptr);
        delete(it->second);
    }
    clusters.clear();
    index = 0;
    groups_ptr = nullptr;
}

cluster_t *ClusterInit::cluster_of(Group *g)
{
    if (g->get_cluster_idx() == (uint64_t)-1)
        return nullptr;

    if (clusters.find(g->get_cluster_idx()) != clusters.end())
        return clusters[g->get_cluster_idx()];

    mtx.lock();
    std::cerr << "Error: cluster with index " << g->get_cluster_idx() << " not found." << std::endl;
    mtx.unlock();

    return nullptr;
}

cluster_t *ClusterInit::create_cluster()
{
    cluster_t *new_c = new cluster_t();

    if (new_c) {
        assert(clusters.find(index) == clusters.end());
        clusters[index] = new_c;
        new_c->set_cluster_id(index);
        index++;
    }

    return new_c;
}

bool ClusterInit::del_cluster(cluster_t *c)
{
    uint64_t index = c->get_cluster_id();
    std::map<uint64_t, cluster_t *>::iterator it = clusters.find(index);

    if (it == clusters.end())
        return false;

    assert(it->second == c);
    clusters.erase(it);
    delete c;
    assert(clusters.find(index) == clusters.end());
    return true;
}

cluster_t * ClusterInit::merge(cluster_t *c1, cluster_t *c2)
{
    c1->append_nodes(c2->get_nodes());
    c1->append_edges(c2->get_edges());
    return c1;
}

void ClusterInit::merge_clusters_of_events(EventBase *prev_e, EventBase *cur_e,
    uint32_t rel_type)
{
    Group *prev_g = groups_ptr->group_of(prev_e);
    Group *cur_g = groups_ptr->group_of(cur_e);

    if (!prev_g || !cur_g) {
        mtx.lock();
        std::cerr << "Check: groups of events do not exist" << std::endl;
        mtx.unlock();
        return;
    }

    //assert(cur_g && prev_g);

    cluster_t *prev_c = cluster_of(prev_g);
    cluster_t *cur_c = cluster_of(cur_g);
    cluster_t *new_c;

    if (prev_c && prev_c == cur_c) {
        prev_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
        return;
    }

    if (prev_c == nullptr) {
        if (cur_c == nullptr) {
            new_c = create_cluster();
            if (!new_c) {
                mtx.lock();
                std::cerr << "OOM " << __func__ << std::endl;
                mtx.unlock();
                return;
            }

            new_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
            new_c->add_node(prev_g);

            if (prev_g != cur_g)
                new_c->add_node(cur_g);

        } else {
            cur_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
            cur_c->add_node(prev_g);
        }
    } else {
        if (cur_c == nullptr) {
            prev_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
            prev_c->add_node(cur_g);
        } else {
            assert(prev_c != cur_c);
            cluster_t *merged_c = merge(prev_c, cur_c);
            merged_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
            assert(merged_c == prev_c);
            cur_c->get_nodes().clear();
            cur_c->get_edges().clear();
            del_cluster(cur_c);
        }
    }
    //TODO: set group root
}

void ClusterInit::merge_by_mach_msg()
{
    mtx.lock();
    std::cerr << "merge cluster via mach_msg pattern\n";
    mtx.unlock();
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> mach_msg_list = groups_ptr->get_list_of_op(MACH_IPC_MSG);
    MsgEvent *curr_msg = nullptr;
    MsgEvent *next_msg = nullptr;
    MsgEvent *peer_msg = nullptr;
    
    for (it = mach_msg_list.begin(); it != mach_msg_list.end(); it++) {
        curr_msg = dynamic_cast<MsgEvent *>(*it);
        assert(curr_msg);
        next_msg = curr_msg->get_next();
        if (next_msg != nullptr) {
            merge_clusters_of_events(curr_msg, next_msg, MSGP_REL);
        } else {
            peer_msg = curr_msg->get_peer();
            if (peer_msg) {
                if (peer_msg->get_abstime() >= curr_msg->get_abstime()) 
                    merge_clusters_of_events(curr_msg, peer_msg, MSGP_REL);
            } else if (!curr_msg->is_freed_before_deliver()) {
                mtx.lock();
                std::cerr << "Check: no peer msg found for : ";
                std::cerr << std::fixed << std::setprecision(2) \
                    << curr_msg->get_abstime() << std::endl;
                mtx.unlock();
            }
        }
    }
    mtx.lock();
    std::cerr << "merge mach msg done\n";
    mtx.unlock();
}

void ClusterInit::merge_by_dispatch_ops()
{
    mtx.lock();
    std::cerr << "merge cluster via dispatch-execute\n";
    mtx.unlock();
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> dispatch_exe_list = groups_ptr->get_list_of_op(DISP_EXE);

    for (it = dispatch_exe_list.begin(); it != dispatch_exe_list.end(); it++) {
        BlockInvokeEvent *invoke_event = dynamic_cast<BlockInvokeEvent *>(*it);
        assert(invoke_event);
        EventBase *root = invoke_event->get_root();
        if (root)
            merge_clusters_of_events(root, invoke_event, DISP_EXE_REL);
    }

    std::list<EventBase *>::iterator deq_it;
    std::list<EventBase *> dispatch_deq_list = groups_ptr->get_list_of_op(DISP_DEQ); 

    for (deq_it = dispatch_deq_list.begin(); deq_it != dispatch_deq_list.end();
        deq_it++) {
        BlockDequeueEvent *deq_event = dynamic_cast<BlockDequeueEvent *>(*deq_it);
        assert(deq_event);
        EventBase *root = deq_event->get_root();
        if (root)
            merge_clusters_of_events(root, deq_event, DISP_EXE_REL);
    }
    mtx.lock();
    std::cerr << "merge dispatch-execute done\n";
    mtx.unlock();
}

void ClusterInit::merge_by_timercallout()
{
    mtx.lock();
    std::cerr << "merge cluster via timercallout\n";
    mtx.unlock();
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> timercallout_list
        = groups_ptr->get_list_of_op(MACH_CALLOUT);

    for (it = timercallout_list.begin(); it != timercallout_list.end(); it++) {
        TimerCalloutEvent *timercallout_event
            = dynamic_cast<TimerCalloutEvent *>(*it);

        assert(timercallout_event);
        TimerCreateEvent *timercreate_event
            = timercallout_event->get_timercreate();

        if (timercreate_event)
            merge_clusters_of_events(timercreate_event,
                timercallout_event,
                CALLOUT_REL);
    }

    std::list<EventBase *>::iterator ct;
    std::list<EventBase *> timercancel_list
        = groups_ptr->get_list_of_op(MACH_CALLCANCEL);

    for (ct = timercancel_list.begin(); ct != timercancel_list.end(); ct++) {
        TimerCancelEvent *timercancel_event
            = dynamic_cast<TimerCancelEvent*>(*ct);

        assert(timercancel_event);
        TimerCreateEvent * timercreate_event
            = timercancel_event->get_timercreate();

        if (timercreate_event)
            merge_clusters_of_events(timercreate_event,
                timercancel_event,
                CALLOUTCANCEL_REL);
    }

    mtx.lock();
    std::cerr << "merge timercallouts done\n";
    mtx.unlock();
}

void ClusterInit::merge_by_ca()
{
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> ca_display_list = groups_ptr->get_list_of_op(CA_DISPLAY);
    for (it = ca_display_list.begin(); it != ca_display_list.end(); it++) {
        CADisplayEvent *cadisplay_event = dynamic_cast<CADisplayEvent*>(*it);
        std::vector<CASetEvent *> casets = cadisplay_event->get_ca_set_events();
        std::vector<CASetEvent *>::iterator set_it;
        for (set_it = casets.begin(); set_it != casets.end(); set_it++) {    
            merge_clusters_of_events(cadisplay_event, *set_it, CA_REL);
        } 
    }
}

void ClusterInit::merge_by_breakpoint_trap()
{
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> breakpoint_trap_list = groups_ptr->get_list_of_op(BREAKPOINT_TRAP);
    for (it = breakpoint_trap_list.begin(); it != breakpoint_trap_list.end(); it++) {
        BreakpointTrapEvent * breakpoint_trap_event = dynamic_cast<BreakpointTrapEvent *>(*it);
        if (breakpoint_trap_event->get_peer() && breakpoint_trap_event->check_read()) {
            merge_clusters_of_events(breakpoint_trap_event, breakpoint_trap_event->get_peer(), BRTRAP_REL);
        }
    }
}

void ClusterInit::merge_by_mkrun()
{
    std::list<EventBase *>::iterator it;
    std::list<EventBase *> mkrun_list = groups_ptr->get_list_of_op(MACH_MK_RUN);
    for (it = mkrun_list.begin(); it != mkrun_list.end(); it++) {
        MakeRunEvent *mkrun_event = dynamic_cast<MakeRunEvent *>(*it);
        if (mkrun_event->get_group_id() != -1 && mkrun_event->get_peer_event()) {
            //clear wait && run_nextreq
            if (mkrun_event->get_mr_type() == CLEAR_WAIT || mkrun_event->get_mr_type() == WORKQ_MR)
                continue;
            merge_clusters_of_events(mkrun_event, mkrun_event->get_peer_event(), MKRUN_REL);
        }
    }

}

void ClusterInit::decode_clusters(std::string & output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t *>::iterator it;
    cluster_t *cur_cluster;

    mtx.lock();
    std::cerr << "Total number of Clusters = " << std::hex << clusters.size() << std::endl;
    mtx.unlock();
    for (it = clusters.begin(); it != clusters.end(); it++) {
        cur_cluster = it->second;
        assert(cur_cluster);
        output << "#Cluster " << std::hex << cur_cluster->get_cluster_id();
        output << "(num of groups = " << std::hex << (cur_cluster->get_nodes()).size() << ")\n";

        cur_cluster->decode_edges(output);
        //cur_cluster->decode_cluster(output);
        //cur_cluster->streamout_cluster(output);
    }
    output.close();
}

void ClusterInit::streamout_clusters(std::string & output_path)
{
    std::ofstream outfile(output_path);
    std::map<uint64_t, cluster_t*>::iterator it;
    cluster_t * cur_cluster;

    for (it = clusters.begin(); it != clusters.end(); it++) {
        cur_cluster = it->second;        
        outfile << "#Cluster " << std::hex << cur_cluster->get_cluster_id();
        outfile << "(num_of_groups = " << std::hex << (cur_cluster->get_nodes()).size() << ")\n";
        cur_cluster->streamout_cluster(outfile);
    }
    outfile.close();
}

void ClusterInit::decode_dangle_groups(std::string & output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, Group *> &gs = groups_ptr->get_groups();
    std::map<uint64_t, Group *>::iterator it;
    uint64_t total_groups = gs.size();
    int dangle_groups = 0;
    int single_groups = 0;

    for (it = gs.begin(); it != gs.end(); it++) {
        Group *g = it->second;
        if (g->get_cluster_idx() == (uint64_t)-1) {
            dangle_groups++;
            if (g->get_container().size() == 1)
                single_groups++;
            output << "Group: " << std::hex <<  g->get_group_id() << std::endl;
            g->decode_group(output);
        }
    }
    output.close();
    mtx.lock();
    std::cerr << "dangle group num = " << std::hex << dangle_groups;
    std::cerr << "(out of " << std::hex << total_groups << " total groups)\n";
    std::cerr << "single group num = " << std::hex << single_groups;
    std::cerr << "(out of " << std::hex << dangle_groups << " dangle groups)\n";
    mtx.unlock();
}
