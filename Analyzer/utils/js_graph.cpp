#include <js_graph.hpp>
/////////////////////////////////////
//create javascript for graph data.
JSGraph::JSGraph(Graph *_graph)
:graph(_graph)
{
    nodes = graph->nodes;
}

void JSGraph::js_lanes(std::vector<Node *>&js_nodes, std::ofstream &outfile)
{
    std::set<std::string> lanes;
    std::vector<Node *>::iterator it;
    for (it = js_nodes.begin(); it != js_nodes.end(); it++) {
        Group *cur_group = (*it)->get_group();
        EventBase *first_event = cur_group->get_first_event();
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

void JSGraph::js_groups(std::vector<Node *>&js_nodes, std::ofstream &outfile)
{
    std::vector<Node *>::iterator it;
    bool comma = false;

    outfile << "var groups = [" << std::endl;
    for (it = js_nodes.begin(); it != js_nodes.end(); it++) {
        
        if (comma)
            outfile << "," << std::endl;
        else
            comma = true;

        Group *cur_group = (*it)->get_group();
        EventBase *first_event = cur_group->get_first_event();
        EventBase *last_event = cur_group->get_last_event();
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
        outfile << std::hex << cur_group->get_group_id() << "\"}";
    }
    outfile << "];" << std::endl;
}

void JSGraph::js_edge(std::ofstream &outfile, EventBase *host, const char *action, EventBase *peer, bool *comma)
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

void JSGraph::message_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    MsgEvent *mach_msg_event = dynamic_cast<MsgEvent *>(event);
    EventBase *peer = nullptr;
    if (!mach_msg_event)
        return;
    if ((peer = mach_msg_event->get_event_peer())) {
        if (mach_msg_event->get_abstime() < peer->get_abstime())
            js_edge(outfile, mach_msg_event, "send", peer, comma);
        else
            js_edge(outfile, mach_msg_event, "recv", nullptr, comma);
    }

    if ((peer = mach_msg_event->get_next()))
        js_edge(outfile, mach_msg_event, "pass_msg", peer, comma);
}

void JSGraph::dispatch_edge(std::ofstream &outfile, EventBase *event, bool *comma)
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

void JSGraph::timercall_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    EventBase *peer = nullptr;
    TimerCreateEvent *timercreate_event = dynamic_cast<TimerCreateEvent *>(event);
    if (timercreate_event && ((peer = timercreate_event->get_called_peer())))
        js_edge(outfile, timercreate_event, "timer_callout", peer, comma);
}

void JSGraph::mkrun_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    MakeRunEvent *mr_event = dynamic_cast<MakeRunEvent *>(event);
    EventBase *peer = nullptr;

    if (mr_event && (peer = mr_event->get_event_peer())) {
        std::string wakeup_resource("wakeup_");
        if (mr_event->get_wait() != nullptr)
            wakeup_resource.append(mr_event->get_wait()->get_wait_resource());
        js_edge(outfile, mr_event, wakeup_resource.c_str(), peer, comma);
    }
}

void JSGraph::wait_label(std::ofstream &outfile, EventBase *event, bool *comma)
{
    WaitEvent *wait_event = dynamic_cast<WaitEvent *>(event);
    if (!wait_event)
        return;
    std::string wait_resource("wait_");
    wait_resource.append(wait_event->get_wait_resource());
    js_edge(outfile, wait_event, wait_resource.c_str(), nullptr, comma);
}

void JSGraph::coreannimation_edge(std::ofstream &outfile, EventBase *event, bool *comma)
{
    CASetEvent *caset_event = dynamic_cast<CASetEvent *>(event);
    EventBase *peer = nullptr;
    if (caset_event && (peer = caset_event->get_display_object()))
        js_edge(outfile, caset_event, "CoreAnimationUpdate", peer, comma);
}

void JSGraph::js_arrows(std::vector<Node *>&js_nodes, std::ofstream &outfile)
{
    std::vector<Node *>::iterator it;
    std::list<EventBase *>::iterator event_it;
    bool comma = false;

    outfile <<"var arrows = [" << std::endl;
    for (it = js_nodes.begin(); it != js_nodes.end(); it++) {
        Group *cur_group = (*it)->get_group();
        std::list<EventBase *> container = cur_group->get_container();
        if (container.size() == 0) {
            std::cerr << "Group " << cur_group->get_group_id() << " has no edges" << std::endl;
            continue;
        }

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
                default:
                    break;
            }
        }
        //js_edge(outfile, last_event, "deactivate", nullptr);
    }
    outfile << "];" << std::endl;
}

void JSGraph::js_cluster(std::vector<Node *>& js_nodes, std::ofstream &outfile)
{
    js_lanes(js_nodes, outfile);
    js_groups(js_nodes, outfile);
    js_arrows(js_nodes, outfile);
}

void JSGraph::js_cluster(std::string path, Node *node)
{
    std::ofstream outfile(path);
    std::vector<Node *> js_nodes;
    std::vector<Node *>::iterator it;
    double begin, end;

    outfile << "var main_lane = \"" << node->get_group()->get_first_event()->get_procname() << "_"\
            << std::hex << node->get_group()->get_first_event()->get_tid() << "\";" << std::endl;
    begin = node->get_group()->get_first_event()->get_abstime();
    end = node->get_group()->get_last_event()->get_abstime();
    js_nodes.clear();

    for (it = nodes.begin(); it != nodes.end(); it++) {
        Group *cur_group = (*it)->get_group();
        if (cur_group->get_size() > 2 && (begin - cur_group->get_last_event()->get_abstime() < 2.5
            || cur_group->get_first_event()->get_abstime() < end))
            js_nodes.push_back(*it);
    }
    js_lanes(js_nodes, outfile);
    js_groups(js_nodes, outfile);
    js_arrows(js_nodes, outfile);
    outfile.close();
}
