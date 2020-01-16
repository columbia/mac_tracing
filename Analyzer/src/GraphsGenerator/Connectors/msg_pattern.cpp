#include "msg_pattern.hpp"
#include "parse_helper.hpp"

#define MSG_PATTERN_DEBUG 0

MsgPattern::MsgPattern(std::list<EventBase*> &list)
:ev_list(list)
{
    patterned_ipcs.clear();
    mig_list.clear();
    msg_list.clear();
    std::list<EventBase *>::iterator it;

    MsgEvent *cur_msg;
    for (it = ev_list.begin(); it != ev_list.end(); it++) {
        cur_msg = dynamic_cast<MsgEvent *> (*it);
        assert(cur_msg);
        if (cur_msg->is_mig() == true)
            mig_list.push_back(cur_msg);
        else {
            msg_list.push_back(cur_msg);
            uint64_t local_port = cur_msg->get_header()->get_local_port();
            if (local_port_msg_list_maps.find(local_port) == local_port_msg_list_maps.end()) {
                std::list<MsgEvent *> temp_list;
                temp_list.clear();
                local_port_msg_list_maps[local_port] = temp_list;
            }
            local_port_msg_list_maps[local_port].push_back(cur_msg);
        }
    }
}
/*
MsgPattern::MsgPattern(std::list<MsgEvent *> &_mig_list, std::list<MsgEvent *> &_msg_list)
:mig_list(_mig_list), msg_list(_msg_list)
{
    patterned_ipcs.clear();
}
*/

MsgPattern::~MsgPattern(void)
{
    patterned_ipcs.clear();
}

void MsgPattern::update_msg_pattern(MsgEvent * event0, MsgEvent * event1,
                                    MsgEvent * event2, MsgEvent* event3)
{
    assert(event0 != nullptr && event1 != nullptr);
    event0->set_peer(event1);
    event1->set_peer(event0);
    if (event2 != nullptr)
        event1->set_next(event2);
    if (event3 != nullptr) {
        assert(event2 != nullptr);
        event2->set_peer(event3);
        event3->set_peer(event2);
    }
    
    msg_episode s;
    s.insert(event0);
    s.insert(event1);
    if (event2 != nullptr) {
        s.insert(event2);
    }
    if (event3 != nullptr) {
        s.insert(event3);
    } 
    patterned_ipcs.push_back(s);
}

void MsgPattern::collect_mig_pattern(void)
{
    std::list<MsgEvent *>::iterator it;
    MsgEvent *cur_msg, *mig_req;
    MsgHeader *curr_header, *mig_req_h;
    
    //if the first msg is recv, discard it
    if (mig_list.front()->get_header()->check_recv() == true)
        mig_list.pop_front();
    //if the last msg is recv, discard it
    if (mig_list.back()->get_header()->check_recv() == false)
        mig_list.pop_back();

    while (!mig_list.empty()) {
        mig_req = mig_list.front();
        mig_list.pop_front();

        if (mig_req->is_freed_before_deliver() == true)
            continue;
       
        mig_req_h = mig_req->get_header();
        //unbalanced matching
        assert(mig_req_h->is_mig());
        assert(mig_req_h->check_recv() == false); 

        for (it = mig_list.begin(); it != mig_list.end(); it++) {
            cur_msg  = *it;
            curr_header = cur_msg->get_header();
            if (curr_header->get_remote_port() ==  mig_req_h->get_local_port()
                && curr_header->get_rport_name() == mig_req_h->get_lport_name()
                && mig_req->get_tid() == cur_msg->get_tid()) {
                update_msg_pattern(mig_req, cur_msg, nullptr, nullptr);
                break;
             }
        }
        //should have a maching recv
        assert(it != mig_list.end());
        it = mig_list.erase(it);
    }
}

void MsgPattern::collect_msg_pattern(void)
{
    std::list<MsgEvent *>::iterator it;
    MsgEvent *cur_msg;
    MsgHeader *cur_header;


    for (it = msg_list.begin(); !msg_list.empty() && it != msg_list.end();) {
        cur_msg = *(it);
        assert(cur_msg);
        cur_header = cur_msg->get_header();
        assert(cur_header);

        if (cur_msg->is_freed_before_deliver() == true
            || cur_header->check_recv() == true) {
            //remove it and continue
            assert(it != msg_list.end() && *it);
            it = msg_list.erase(it);
            if (it != msg_list.end())
                assert(*(it) != cur_msg);
            continue;
        }

        if (cur_header->get_remote_port() != 0 && cur_header->get_local_port() != 0) {
            std::list<MsgEvent *>::iterator req_send_offset = it;
            std::list<MsgEvent *>::iterator req_recv_offset;
            std::list<MsgEvent *>::iterator reply_send_offset;
            std::list<MsgEvent *>::iterator reply_recv_offset;
            
            uint32_t reply_sender_port_name = 0;
            uint32_t reply_recver_port_name = cur_header->get_lport_name();
            pid_t request_pid = cur_msg->get_pid();
            pid_t reply_pid = -1;
        
            req_recv_offset = search_ipc_msg(
                                &reply_sender_port_name,
                                &reply_pid,
                                cur_header->get_remote_port(),
                                cur_header->get_local_port(),
                                true,
                                req_send_offset,
                                reply_recver_port_name);

            if (req_recv_offset == msg_list.end()) {
                assert(it != msg_list.end() && *it);
                it = msg_list.erase(it);
                if (it != msg_list.end())
                    assert(*(it) != cur_msg);
                continue;
            }

            reply_send_offset = search_ipc_msg(
                                &reply_sender_port_name,
                                &reply_pid,
                                cur_header->get_local_port(),
                                0,
                                false,
                                req_recv_offset,
                                reply_recver_port_name);
            if (reply_send_offset == msg_list.end()) {
                update_msg_pattern(*it, *req_recv_offset, nullptr, nullptr);
                assert(req_recv_offset != msg_list.end() && *req_recv_offset);
                msg_list.erase(req_recv_offset);
                assert(it != msg_list.end() && *it);
                it = msg_list.erase(it);
                if (it != msg_list.end())
                    assert(*(it) != cur_msg);
                //TODO: checking if two sends connect to the same recv later
                continue;
            }

            reply_recv_offset = search_ipc_msg(
                                &reply_recver_port_name,
                                &request_pid,
                                cur_header->get_local_port(),
                                0,
                                true,
                                reply_send_offset,
                                reply_recver_port_name);
            if (reply_recv_offset == msg_list.end()) { 
                update_msg_pattern(*it, *req_recv_offset, nullptr, nullptr);
                //msg_list.erase(reply_send_offset);
                assert(req_recv_offset != msg_list.end() && *req_recv_offset);
                msg_list.erase(req_recv_offset);
                assert(it != msg_list.end() && *it);
                it = msg_list.erase(it);
                if (it != msg_list.end())
                    assert(*(it) != cur_msg);
                continue;
            }

            update_msg_pattern(*it, *req_recv_offset, *reply_send_offset, *reply_recv_offset);
            assert(*reply_recv_offset);
            msg_list.erase(reply_recv_offset);
            assert(*reply_send_offset);
            msg_list.erase(reply_send_offset);
            assert(*req_recv_offset);
            msg_list.erase(req_recv_offset);
            it = msg_list.erase(it);
            if (it != msg_list.end())
                assert(*(it) != cur_msg);
        }
        else if (cur_header->get_remote_port() != 0 && cur_header->get_local_port() == 0) {
            std::list<MsgEvent *>::iterator req_send_offset = it;
            std::list<MsgEvent *>::iterator req_recv_offset;
            uint32_t reply_sender_port_name = 0;
            pid_t reply_pid = -1;

            req_recv_offset = search_ipc_msg(
                                &reply_sender_port_name,
                                &reply_pid,
                                cur_header->get_remote_port(),
                                cur_header->get_local_port(),
                                true,
                                req_send_offset,
                                0);
            if (req_recv_offset == msg_list.end()) {
                assert(*it);
                it = msg_list.erase(it);
                if (it != msg_list.end())
                    assert(*(it) != cur_msg);
                continue;
            }
            update_msg_pattern(*it, *req_recv_offset, nullptr, nullptr);
            assert(*req_recv_offset);
            msg_list.erase(req_recv_offset);
            assert(*it);
            it = msg_list.erase(it);
            if (it != msg_list.end())
                assert(*(it) != cur_msg);
        }
        else {
            assert(*it);
            it = msg_list.erase(it);
            if (it != msg_list.end())
                assert(*(it) != cur_msg);
        }
    }

#ifdef MSG_PATTERN_DEBUG
    mtx.lock();
    std::cerr << "finish msg pattern search." << std::endl;
    std::cerr << "total number of msg patterns: " << patterned_ipcs.size() << std::endl;
    mtx.unlock();
#endif
    std::string output("output/msg_pattern.log");
    decode_patterned_ipcs(output);
}

void MsgPattern::collect_patterned_ipcs()
{
    //collect_mig_pattern();
    collect_msg_pattern();
}

/*
 * parameters for function search_ipc_msg
 * RECV1
 * (out)port_name : get reply port name in receiver side(local port from sender)
 * (in)remote_port : remote port kaddr
 * (in)local_port : local port kaddr
 * (in)begin_it : search begins from the next iterator
 * (out)i : return the result ipc msg_event pos in the list
 * REPLY (RECV/SEND):
 * (in)port_name : reply port name in reply-sender or reply-receiver(remote port in the reply)
 */
std::list<MsgEvent *>::iterator MsgPattern::search_ipc_msg(
            uint32_t *port_name, 
            pid_t *pid,
            uint64_t remote_port, uint64_t local_port,
            bool is_recv,
            std::list<MsgEvent *>::iterator begin_it,
            uint32_t reply_recver_port_name)
{
    std::list<MsgEvent *>::iterator cur_it = begin_it;

    for(cur_it++; cur_it != msg_list.end(); ++cur_it) {
        MsgEvent *cur_ipc = *cur_it;
        assert(cur_ipc);
        MsgHeader *header = cur_ipc->get_header();
        assert(header);

        if (header->check_recv() == is_recv
            && header->get_remote_port() == remote_port
            && header->get_local_port() == local_port) {
            /*checking processes*/
            if (*pid == -1) {
                *pid = cur_ipc->get_pid();
            } else {
                if (*pid != cur_ipc->get_pid()) {
                    continue;
                }
            }
            /*checking port names*/
            if (*port_name == 0) {
                assert(is_recv == true);
                *port_name = header->get_lport_name();
            } else {
                if (is_recv == false && header->is_from_kernel()) {
                    if (header->get_rport_name() != reply_recver_port_name) {
                        continue;
                    }
                } else { //recv == true || not from kernel
                    if (*port_name != header->get_rport_name()) {
                        continue;
                    }
                }
            }
            /*meet requirment*/
            return cur_it;
        }
    }
    return cur_it;
}

std::list<msg_episode> & MsgPattern::get_patterned_ipcs(void)
{
    return patterned_ipcs;
}

std::list<msg_episode>::iterator MsgPattern::episode_of(MsgEvent *msg_event)
{
    if (msg_event == nullptr)
        return patterned_ipcs.end();

    std::list<msg_episode>::iterator it;
    for (it = patterned_ipcs.begin(); it != patterned_ipcs.end(); it++) {
        if ((*it).find(msg_event) != (*it).end())
            return it;
    }
    return it;
}

std::vector<MsgEvent *> MsgPattern::sort_msg_episode(msg_episode & s)
{
    std::vector<MsgEvent*> episode;
    episode.clear();
    std::set<MsgEvent*>::iterator s_it;
    for (s_it = s.begin(); s_it != s.end(); s_it++) {
        episode.push_back(*s_it);
    }
    sort(episode.begin(), episode.end(), Parse::EventComparator::compare_time);
    return episode;
}

void MsgPattern::decode_patterned_ipcs(std::string &output_path)
{
    std::ofstream output(output_path, std::ofstream::out);
    std::list<msg_episode>::iterator it;
    int i = 0;
    output << "checking mach msg pattern ... " << std::endl;
    output << "number of patterns " << patterned_ipcs.size() << std::endl;
    for (it = patterned_ipcs.begin(); it != patterned_ipcs.end(); it++, i++) {
        output << "## " << i <<" size = " << (*it).size() << std::endl;
        std::vector<MsgEvent *> cur = sort_msg_episode(*it);
        std::vector<MsgEvent *>::iterator s_it;
        for (s_it = cur.begin(); s_it != cur.end(); s_it++) {
            (*s_it)->decode_event(0, output);
        }
    }
    output << "mach msg checking done" << std::endl;
    output.close();
}

void MsgPattern::verify_msg_pattern()
{
    
}
