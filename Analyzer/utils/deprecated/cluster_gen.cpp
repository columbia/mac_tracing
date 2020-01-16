#include "parser.hpp"
#include "cluster.hpp"

#define DEBUG_EXTRACT_CLUSTER 1

ClusterGen::ClusterGen(Groups *groups, bool connect_mkrun)
{
    groups_ptr = groups;
    std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
    std::map<uint64_t, Group *>::iterator it;

    std::cerr << "Sizeof main_groups " << main_groups.size() << std::endl;
    for (it = main_groups.begin(); it != main_groups.end(); it++) {
        NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(it->second->contain_nsappevent());
        if (init_event && init_event->is_begin() == false) {
            if (it->second->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
                //TODO: decode the user event for checking
                std::cerr << "Check: group with group id " << std::hex << it->second->get_group_id()\
                    << " has been added into cluster " << std::hex << it->second->get_cluster_idx() << std::endl;
#endif
                continue;
            }
            cluster_t *new_cluster = init_cluster(it->second);
            clusters[it->first] = new_cluster;

#ifdef DEBUG_EXTRACT_CLUSTER
            std::cerr << "Augment cluster from group " <<  it->second->get_group_id() << ", with root event at "\
                << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
#endif
            augment_cluster(new_cluster, connect_mkrun);
            add_waitsync(new_cluster);
        } else {

#if DEBUG_EXTRACT_CLUSTER
            if (it->second->get_cluster_idx() == -1) {
                std::cerr << "Check: group from main thread " << std::hex << it->second->get_group_id()\
                    << " was neither augmented as root nor added to clusters" << std::endl;
            }
#endif
        }
    }
}

ClusterGen::~ClusterGen()
{
    std::map<uint64_t, cluster_t *>::iterator cluster_it;
    for (cluster_it = clusters.begin(); cluster_it != clusters.end(); cluster_it++) {
        if (cluster_it->second)
            delete(cluster_it->second);
    }
    clusters.clear();
}

cluster_t *ClusterGen::init_cluster(Group *group)
{
    cluster_t *ret = new Cluster(group);
    ret->push_connectors(group, nullptr);
    return ret;
}

cluster_t *ClusterGen::cluster_of(Group *g)
{
    if (!g || g->get_cluster_idx() == (uint64_t)-1)
        return nullptr;

    if (clusters.find(g->get_cluster_idx()) != clusters.end())
        return clusters[g->get_cluster_idx()];

    mtx.lock();
    std::cerr << "Error: cluster with index " << g->get_cluster_idx() << " not found." << std::endl;
    mtx.unlock();

    return nullptr;
}

void ClusterGen::merge_by_mach_msg(cluster_t *cluster, MsgEvent *curr_msg)
{
    MsgEvent *next_msg = curr_msg->get_next();

    if (next_msg) {
        Group *next_group = groups_ptr->group_of(next_msg);
        assert(next_group);

#if DEBUG_EXTRACT_CLUSTER
        {
        Group *peer_group = next_group;
        std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
        if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
                && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
            NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
            mtx.lock();
            if (init_event && init_event->is_begin() == false)
                std::cerr << "Event at " << std::fixed << std::setprecision(1) << curr_msg->get_abstime()\
                    << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                    << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
            mtx.unlock();
        }
        }
#endif

        if (next_group->get_cluster_idx() == cluster->get_cluster_id()) {
            cluster->add_edge(groups_ptr->group_of(curr_msg),
                next_group,
                curr_msg,
                next_msg,
                MSGP_REL);
        } else if (next_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
                << " event at " << std::fixed << std::setprecision(1) << curr_msg->get_abstime() << std::endl;
            std::cerr << "group " << std::hex << next_group->get_group_id() \
                << " with event at " << std::fixed << std::setprecision(1) << next_msg->get_abstime() \
                << " was added into cluster " << std::hex << next_group->get_cluster_idx() << std::endl;
            mtx.unlock();
#endif
        } else {
            cluster->add_edge(groups_ptr->group_of(curr_msg),
                next_group,
                curr_msg,
                next_msg,
                MSGP_REL);
            if (cluster->add_node(next_group))
                cluster->push_connectors(next_group, next_msg);
        }
    }

    MsgEvent *peer_msg = curr_msg->get_peer();
    if (peer_msg) {
        Group *peer_group = groups_ptr->group_of(peer_msg);
        if (peer_group == nullptr) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": event at " << std::fixed << std::setprecision(1) \
                << peer_msg->get_abstime() << " is not in group properly" << std::endl;
            mtx.unlock();
#endif
        }
        assert(peer_group);

#if DEBUG_EXTRACT_CLUSTER
        std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
        if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
                && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
            NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
            mtx.lock();
            if (init_event && init_event->is_begin() == false)
                std::cerr << "Event at " << std::fixed << std::setprecision(1) << curr_msg->get_abstime()\
                    << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                    << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
            mtx.unlock();
        }
#endif
        if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
            if (peer_msg->get_abstime() > curr_msg->get_abstime())
                cluster->add_edge(groups_ptr->group_of(curr_msg),
                    peer_group,
                    curr_msg,
                    peer_msg,
                    MSGP_REL);
            else
                cluster->add_edge(peer_group,
                    groups_ptr->group_of(curr_msg),
                    peer_msg,
                    curr_msg,
                    MSGP_REL);
        } else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
                << " event at " << std::fixed << std::setprecision(1) << curr_msg->get_abstime() << std::endl;
            std::cerr << "group " << std::hex << peer_group->get_group_id() \
                << " with event at " << std::fixed << std::setprecision(1) << peer_msg->get_abstime() \
                << " was added into cluster " << std::hex << peer_group->get_cluster_idx() << std::endl;
            mtx.unlock();
#endif
        } else {
            if (peer_msg->get_abstime() > curr_msg->get_abstime())
                cluster->add_edge(groups_ptr->group_of(curr_msg),
                    peer_group,
                    curr_msg,
                    peer_msg,
                    MSGP_REL);
            else
                cluster->add_edge(peer_group,
                    groups_ptr->group_of(curr_msg),
                    peer_msg,
                    curr_msg,
                    MSGP_REL);
            if (cluster->add_node(peer_group))
                cluster->push_connectors(peer_group, peer_msg);
        }
    }
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster, BlockEnqueueEvent *enqueue)
{
    if (!enqueue->is_consumed())
        return;
    EventBase *deq = enqueue->get_consumer();
    if (!deq) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << "Error: no dequeue event for enqueue but with comsume flag set\n";
        mtx.unlock();
#endif
        return;
    }

    Group *deq_group = groups_ptr->group_of(deq);
    if (deq_group == nullptr) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << "No grouped deq event at " << std::fixed << std::setprecision(1) << deq->get_abstime() << std::endl;
        mtx.unlock();
#endif
        return;
    }

#if DEBUG_EXTRACT_CLUSTER
    Group *peer_group = deq_group;
    std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
    if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
        NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
        mtx.lock();
        if (init_event && init_event->is_begin() == false)
            std::cerr << "Event at " << std::fixed << std::setprecision(1) << enqueue->get_abstime()\
                << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
        mtx.unlock();
    }
#endif

    if (deq_group->get_cluster_idx() == cluster->get_cluster_id()) {
        cluster->add_edge(groups_ptr->group_of(enqueue),
                deq_group,
                enqueue,
                deq,
                DISP_DEQ_REL);
    } else if (deq_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
            << " event at " << std::fixed << std::setprecision(1) << enqueue->get_abstime() << std::endl;
        std::cerr << " group " << std::hex << deq_group->get_group_id() \
            << " with event at " << std::fixed << std::setprecision(1) << deq->get_abstime() \
            << " was added into cluster " << std::hex << deq_group->get_cluster_idx() << std::endl;
        mtx.unlock();
#endif
    } else {
        cluster->add_edge(groups_ptr->group_of(enqueue),
                deq_group,
                enqueue,
                deq,
                DISP_DEQ_REL);
        if (cluster->add_node(deq_group))
            cluster->push_connectors(deq_group, deq);
    }
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster, BlockDequeueEvent *dequeue)
{
    EventBase *invoke = dequeue->get_invoke();
    if (!invoke)
        return;
    Group *invoke_group = groups_ptr->group_of(invoke);

#if DEBUG_EXTRACT_CLUSTER
    Group *peer_group = invoke_group;
    std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
    if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
        NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
        mtx.lock();
        if (init_event && init_event->is_begin() == false)
            std::cerr << "Event at " << std::fixed << std::setprecision(1) << dequeue->get_abstime()\
                << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
        mtx.unlock();
    }
#endif
    if (invoke_group->get_cluster_idx() == cluster->get_cluster_id()) {
        cluster->add_edge(groups_ptr->group_of(dequeue),
                invoke_group,
                dequeue,
                invoke,
                DISP_EXE_REL);
    } else if (invoke_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
            << " event at " << std::fixed << std::setprecision(1) << dequeue->get_abstime() << std::endl;
        std::cerr << " group " << std::hex << invoke_group->get_group_id() \
            << " with event at " << std::fixed << std::setprecision(1) << invoke->get_abstime() \
            << " was added into cluster " << std::hex << invoke_group->get_cluster_idx() << std::endl;
        mtx.unlock();
#endif
    } else {
        cluster->add_edge(groups_ptr->group_of(dequeue),
                invoke_group,
                dequeue,
                invoke,
                DISP_EXE_REL);
        if (cluster->add_node(invoke_group))
            cluster->push_connectors(invoke_group, invoke);
    }
}

void ClusterGen::merge_by_timercallout(cluster_t *cluster,
        TimerCreateEvent *timercreate_event)
{
    if (!timercreate_event->check_event_processing())
        return;

    TimerCalloutEvent *timercallout_event = timercreate_event->get_called_peer();
    if (!timercallout_event)
        return;

    Group *timercallout_group = groups_ptr->group_of(timercallout_event);

#if DEBUG_EXTRACT_CLUSTER
    Group *peer_group = timercallout_group;
    std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
    if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
        NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
        mtx.lock();
        if (init_event && init_event->is_begin() == false)
            std::cerr << "Event at " << std::fixed << std::setprecision(1) << timercreate_event->get_abstime()\
                << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
        mtx.unlock();
    }
#endif

    if (timercallout_group->get_cluster_idx() == cluster->get_cluster_id()) {
        cluster->add_edge(groups_ptr->group_of(timercreate_event),
                timercallout_group,
                timercreate_event,
                timercallout_event,
                CALLOUT_REL);
    } else if (timercallout_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
            << " event at " << std::fixed << std::setprecision(1) << timercreate_event->get_abstime() << std::endl;
        std::cerr << " group " << std::hex << timercallout_group->get_group_id() \
            << " with event at " << std::fixed << std::setprecision(1) << timercallout_event->get_abstime() \
            << " was added into cluster " << std::hex << timercallout_group->get_cluster_idx() << std::endl;
        mtx.unlock();
#endif
    } else {
        cluster->add_edge(groups_ptr->group_of(timercreate_event),
                timercallout_group,
                timercreate_event,
                timercallout_event,
                CALLOUT_REL);
        if (cluster->add_node(timercallout_group))
            cluster->push_connectors(timercallout_group, timercallout_event);
    }
}

void ClusterGen::merge_by_coreanimation(cluster_t *cluster,
        CASetEvent *caset_event)
{
    Group *caset_group = groups_ptr->group_of(caset_event);
    CADisplayEvent *cadisplay_event = caset_event->get_display_object();

    if (!cadisplay_event)
        return;
    Group *cadisplay_group = groups_ptr->group_of(cadisplay_event);

#if DEBUG_EXTRACT_CLUSTER
    Group * peer_group = cadisplay_group;
    std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
    if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
        NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
        mtx.lock();
        if (init_event && init_event->is_begin() == false)
            std::cerr << "Event at " << std::fixed << std::setprecision(1) << caset_event->get_abstime()\
                << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
        mtx.unlock();
    }
#endif

    if (cadisplay_group->get_cluster_idx() == cluster->get_cluster_id()) {
        cluster->add_edge(caset_group,
                cadisplay_group,
                caset_event,
                cadisplay_event,
                CA_REL);
    } else if (cadisplay_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
            << " event at " << std::fixed << std::setprecision(1) << caset_event->get_abstime() << std::endl;
        std::cerr << " group " << std::hex << cadisplay_group->get_group_id() \
            << " with event at " << std::fixed << std::setprecision(1) << cadisplay_event->get_abstime() \
            << " was added into cluster " << std::hex << cadisplay_group->get_cluster_idx() << std::endl;
        mtx.unlock();
#endif
    } else {
        cluster->add_edge(caset_group,
                cadisplay_group,
                caset_event,
                cadisplay_event,
                CA_REL);
        if (cluster->add_node(cadisplay_group))
            cluster->push_connectors(cadisplay_group, cadisplay_event);
    }
}

void ClusterGen::merge_by_sharevariables(cluster_t *cluster, 
        BreakpointTrapEvent *breakpoint_event)
{
    BreakpointTrapEvent *peer_event = breakpoint_event->get_peer();
    Group *read_group,  *write_group, *peer_group;

    if (!peer_event)
        return;
    
    if (peer_event->check_read()) {
        peer_group = read_group = groups_ptr->group_of(peer_event);
        write_group = groups_ptr->group_of(breakpoint_event);
    } else {
        peer_group = write_group = groups_ptr->group_of(peer_event);
        read_group = groups_ptr->group_of(breakpoint_event);
    }
    assert(read_group && write_group);

#if DEBUG_EXTRACT_CLUSTER
        std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
        if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
            NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
            mtx.lock();
            if (init_event && init_event->is_begin() == false)
                std::cerr << "Event at " << std::fixed << std::setprecision(1) << breakpoint_event->get_abstime()\
                    << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                    << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
            mtx.unlock();
        }
#endif

    if (read_group->get_cluster_idx() == write_group->get_cluster_idx()) {
        cluster->add_edge(write_group,
                read_group,
                breakpoint_event,
                peer_event,
                BRTRAP_REL);
    } else if (write_group->get_cluster_idx() != -1 && read_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
        mtx.lock();
        std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
            << " event at " << std::fixed << std::setprecision(1) << breakpoint_event->get_abstime();
        std::cerr << " group " << std::hex << peer_group->get_group_id() \
            << " with event at " << std::fixed << std::setprecision(1) << peer_event->get_abstime() \
            << " was added into cluster " << std::hex << peer_group->get_cluster_idx() << std::endl;
        mtx.unlock();
#endif
    } else {
        cluster->add_edge(write_group,
                read_group,
                breakpoint_event,
                peer_event,
                BRTRAP_REL);
        if (cluster->add_node(peer_group))
            cluster->push_connectors(peer_group, peer_event);
    }
}

void ClusterGen::merge_by_rlworks(cluster_t *cluster, RunLoopBoundaryEvent *rlboundary_event)
{
    EventBase *peer_event = nullptr;
    Group *peer_group = nullptr, *cur_group = groups_ptr->group_of(rlboundary_event);

    if (!(peer_event = rlboundary_event->get_owner()))
        return;
    
    if (!(peer_group = groups_ptr->group_of(peer_event)))
        return;

    if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
        cluster->add_edge(cur_group,
            peer_group,
            rlboundary_event,
            peer_event,
            RLITEM_REL);
    } else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
                << " event at " << std::fixed << std::setprecision(1) << rlboundary_event->get_abstime() << std::endl;
            std::cerr << " group " << std::hex << peer_group->get_group_id() \
                << " with event at " << std::fixed << std::setprecision(1) << peer_event->get_abstime() \
                << " was added into cluster " << std::hex << peer_group->get_cluster_idx() << std::endl;
            mtx.unlock();
#endif
    } else {
        cluster->add_edge(cur_group,
            peer_group,
            rlboundary_event,
            peer_event,
            RLITEM_REL);
        if (cluster->add_node(peer_group))
            cluster->push_connectors(peer_group, peer_event);
    }
}

void ClusterGen::merge_by_mkrunnable(cluster_t *cluster, MakeRunEvent *wakeup_event)
{
    if (wakeup_event->get_mr_type() == CLEAR_WAIT
        || wakeup_event->get_mr_type() == WORKQ_MR)
        return;

    EventBase *peer = nullptr;

    if (wakeup_event && (peer = wakeup_event->get_peer_event())) {
        if (!groups_ptr->group_of(peer)) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << "Check: no event gets woken by mkrunnable event at" ;
            std::cerr << std::fixed << std::setprecision(1) << wakeup_event->get_abstime() << std::endl;
            mtx.unlock();
#endif
            return;
        }

        Group *wakeup_group = groups_ptr->group_of(wakeup_event);
        Group *peer_group = groups_ptr->group_of(peer);

#if DEBUG_EXTRACT_CLUSTER
        std::map<uint64_t, Group *> main_groups = groups_ptr->get_main_groups();
        if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
            && main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
            NSAppEventEvent *init_event = dynamic_cast<NSAppEventEvent *>(peer_group->contain_nsappevent());
            mtx.lock();
            if (init_event && init_event->is_begin() == false)
                std::cerr << "Event at " << std::fixed << std::setprecision(1) << wakeup_event->get_abstime()\
                    << " Tries to connect to Root Group " << std::hex << peer_group->get_group_id()\
                    << " with app event at " << std::fixed << std::setprecision(1) << init_event->get_abstime() << std::endl;
            mtx.unlock();
        }
#endif
        if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
            cluster->add_edge(wakeup_group,
                peer_group,
                wakeup_event,
                peer,
                MKRUN_REL);
        } else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
                << " event at " << std::fixed << std::setprecision(1) << wakeup_event->get_abstime() << std::endl;
            std::cerr << " group " << std::hex << peer_group->get_group_id() \
                << " with event at " << std::fixed << std::setprecision(1) << peer->get_abstime() \
                << " was added into cluster " << std::hex << peer_group->get_cluster_idx() << std::endl;
            mtx.unlock();
#endif
        } else {
            cluster->add_edge(wakeup_group,
                peer_group,
                wakeup_event,
                peer,
                MKRUN_REL);
            if (cluster->add_node(peer_group))
                cluster->push_connectors(peer_group, peer);
        }
    }
}

void ClusterGen::augment_cluster(cluster_t *cur_cluster, bool connect_mkrun)
{
    std::list<EventBase *> connectors = cur_cluster->pop_cur_connectors();
    if (connectors.size() == 0)
        return;

    std::list<EventBase *>::iterator it;
    EventBase *event;
    for (it = connectors.begin(); it != connectors.end(); it++) {
        event = *it;

        switch (event->get_event_type()) {
            case MSG_EVENT:
                merge_by_mach_msg(cur_cluster, dynamic_cast<MsgEvent *>(event));
                break;
            case DISP_ENQ_EVENT:
                merge_by_dispatch_ops(cur_cluster, dynamic_cast<BlockEnqueueEvent *>(event));
                break;
            case DISP_DEQ_EVENT:
                merge_by_dispatch_ops(cur_cluster, dynamic_cast<BlockDequeueEvent *>(event));
                break;
            case TMCALL_CREATE_EVENT:
                merge_by_timercallout(cur_cluster, dynamic_cast<TimerCreateEvent *>(event));
                break;
            case CA_SET_EVENT:
                merge_by_coreanimation(cur_cluster, dynamic_cast<CASetEvent *>(event));
                break;
            case BREAKPOINT_TRAP_EVENT:
                merge_by_sharevariables(cur_cluster, dynamic_cast<BreakpointTrapEvent *>(event));
                break;
            case RL_BOUNDARY_EVENT:    
                merge_by_rlworks(cur_cluster, dynamic_cast<RunLoopBoundaryEvent *>(event));
                break;
            case MR_EVENT:
                if (connect_mkrun)
                    merge_by_mkrunnable(cur_cluster, dynamic_cast<MakeRunEvent *>(event));
                break;
            default:
#if DEBUG_EXTRACT_CLUSTER
                mtx.lock();
                std::cerr << "unknown connector event #" << event->get_event_type() << std::endl;
                mtx.unlock();
#endif
                break;
        }
    }
    augment_cluster(cur_cluster, connect_mkrun);
}

void ClusterGen::merge_by_waitsync(cluster_t *cluster, WaitEvent *wait_event)
{
    MakeRunEvent *wakeup_event = wait_event->get_mkrun();
    EventBase *peer = nullptr;

    if (wakeup_event && (peer = wakeup_event->get_peer_event())) {
        if (!groups_ptr->group_of(peer)) {
#if 0 //DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << "No succesive execution gets grouped in the thread at" ;
            std::cerr << std::fixed << std::setprecision(1) << wait_event->get_abstime() << std::endl;
            mtx.unlock();
#endif
            return;
        }
        
        if (groups_ptr->group_of(peer)->get_cluster_idx() != cluster->get_cluster_id()) {
#if 0 //DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << "No succesive execution inside cluster after wait at ";
            std::cerr << std::fixed << std::setprecision(1) << wait_event->get_abstime() << std::endl;
            mtx.unlock();
#endif
            return;
        }

        Group *wakeup_group = groups_ptr->group_of(wakeup_event);
        if (wakeup_group->get_cluster_idx() == cluster->get_cluster_id()) {
            cluster->add_edge(wakeup_group,
                groups_ptr->group_of(peer),
                wakeup_event,
                peer,
                MKRUN_REL);
        } else if (wakeup_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
            mtx.lock();
            std::cerr << __func__ <<": cluster " << std::hex << cluster->get_cluster_id() \
                << " event at " << std::fixed << std::setprecision(1) << peer->get_abstime() << std::endl;
            std::cerr << "group " << std::hex << wakeup_group->get_group_id() \
                << " with event at " << std::fixed << std::setprecision(1) << wakeup_event->get_abstime() \
                << " was added into cluster " << std::hex << wakeup_group->get_cluster_idx() << std::endl;
            mtx.unlock();
#endif
        } else {
            cluster->add_edge(wakeup_group,
                groups_ptr->group_of(peer),
                wakeup_event,
                peer,
                MKRUN_REL);
            cluster->add_node(wakeup_group);
        }
    }
}

void ClusterGen::add_waitsync(cluster_t *cluster)
{
    std::list<WaitEvent *>::iterator it;
    std::list<WaitEvent *> wait_events = cluster->get_wait_events();
    for (it = wait_events.begin(); it != wait_events.end(); it++) {
        merge_by_waitsync(cluster, *it);
    }
}

void ClusterGen::js_clusters(std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t*>::iterator it;
    uint64_t index = 0;
    cluster_t * cur_cluster;

    mtx.lock();
    std::cerr << "Total number of Clusters = " << std::hex << clusters.size() << std::endl;
    mtx.unlock();

    for (it = clusters.begin(); it != clusters.end(); it++) {
        cur_cluster = it->second;
        assert(cur_cluster != nullptr);
        index = cur_cluster->get_cluster_id();
        output << "#Cluster " << std::hex << cur_cluster->get_cluster_id();
        output << "(num of groups = " << std::hex << (cur_cluster->get_nodes()).size() << ")\n";
        cur_cluster->js_cluster(output);
    }

    output.close();
}

void ClusterGen::streamout_clusters(std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t*>::iterator it;
    cluster_t * cur_cluster;

    mtx.lock();
    std::cerr << "Total number of Clusters = " << std::hex << clusters.size() << std::endl;
    mtx.unlock();

    for (it = clusters.begin(); it != clusters.end(); it++) {
        cur_cluster = it->second;
        assert(cur_cluster != nullptr);
        if (cur_cluster->get_nodes().size() < 10)
            continue;
        output << "#Cluster " << std::hex << cur_cluster->get_cluster_id();
        output << "(num of groups = " << std::hex << (cur_cluster->get_nodes()).size() << ")\n";

        cur_cluster->decode_edges(output);
        cur_cluster->streamout_cluster(output);
    }

    output.close();
}

void ClusterGen::inspect_clusters(std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t*>::iterator it;
    cluster_t * cur_cluster;

    mtx.lock();
    std::cerr << "Total number of Clusters = " << std::hex << clusters.size() << std::endl;
    mtx.unlock();

    for (it = clusters.begin(); it != clusters.end(); it++) {
        cur_cluster = it->second;
        if (cur_cluster->get_nodes().size() < 10)
            continue;
        output << "#Cluster " << std::hex << cur_cluster->get_cluster_id() << std::endl;
        cur_cluster->inspect_procs_irrelevance(output);
    }
    output.close();
}

void ClusterGen::compare(ClusterGen *peer, std::string &output_path)
{
    std::ofstream output(output_path);
    std::map<uint64_t, cluster_t *> peer_clusters = peer->get_clusters();
    std::map<uint64_t, cluster_t *>::iterator it1, it2;

    for (it1 = clusters.begin(); it1 != clusters.end(); it1++) {
        cluster_t *cur_cluster = it1->second;
        for (it2 = peer_clusters.begin(); it2 != peer_clusters.end(); it2++) {
            if (it1->first == it2->first) {
                cur_cluster->compare(it2->second, output);
            }
        }
    }
    output.close();
}

void ClusterGen::check_connection(uint64_t gid_1, uint64_t gid_2, std::string &output_path)
{
    std::ofstream output;
    output.open(output_path, std::ofstream::out | std::ofstream::app);
    //std::ofstream output(output_path, std::ofstream::app);
    Group *group_1, *group_2;
    cluster_t *cluster;

    group_1 = groups_ptr->get_groups()[gid_1];
    group_2 = groups_ptr->get_groups()[gid_2];
    
    output << "+++++\ncheck_connection of group " << std::hex << gid_1 << " and group " << std::hex << gid_2 << std::endl;
    cluster = cluster_of(group_1);
    if (!cluster || cluster_of(group_2) != cluster)  {
        output << "group " << std::hex << gid_1 << " and group " << std::hex << gid_2 << " are not in the same cluster" << std::endl;
    } else {
        cluster->check_connection(group_1, group_2, output);
    }
    output.close();
}
