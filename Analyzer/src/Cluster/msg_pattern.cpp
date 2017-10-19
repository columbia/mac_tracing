#include "msg_pattern.hpp"
#include "parse_helper.hpp"

#define MSG_PATTERN_DEBUG 1

MsgPattern::MsgPattern(list<event_t*> & list)
:ev_list(list)
{
	patterned_ipcs.clear();
}

MsgPattern::~MsgPattern(void)
{
	patterned_ipcs.clear();
}

void MsgPattern::update_msg_pattern(msg_ev_t * event0, msg_ev_t * event1,
									msg_ev_t * event2, msg_ev_t* event3)
{
	assert(event0 != NULL && event1 != NULL);
	event0->set_peer(event1);
	event1->set_peer(event0);
	if (event2 != NULL)
		event1->set_next(event2);
	if (event3 != NULL) {
		assert(event2 != NULL);
		event2->set_peer(event3);
		event3->set_peer(event2);
	}
	
	msg_episode s;
	s.insert(event0);
	s.insert(event1);
	if (event2 != NULL) {
		s.insert(event2);
	}
	if (event3 != NULL) {
		s.insert(event3);
	} 
	patterned_ipcs.push_back(s);
}

void MsgPattern::collect_patterned_ipcs(void)
{
	int size = ev_list.size();
	list<event_t *>::iterator it;
	int i = 0;
	bool * mark_visit = (bool*)malloc(size * sizeof(bool));

	if (mark_visit == NULL) {
		cerr << "Error: OOM in msg pattern analysis\n";
		exit(EXIT_FAILURE);
	}
	memset(mark_visit, 0, size * sizeof(bool));

#ifdef MSG_PATTERN_DEBUG
	cerr << "begin msg pattern searching ... list size " << dec << ev_list.size() << endl; 
#endif
	
	for (it = ev_list.begin(); it != ev_list.end(); ++it, ++i) {
		msg_ev_t * cur_msg = dynamic_cast<msg_ev_t*>(*it);

		if (cur_msg->is_freed_before_deliver() == true)
			continue;

		if (mark_visit[i] == true)
			continue;
		
		msgh_t * header = cur_msg->get_header();
		if (header->check_recv() == true)
			continue;

		if (header->is_mig() == true) {// special matching for mig
			if (header->get_local_port() == 0)
				continue;
			list<event_t *>::iterator req_send_offset = it;
			list<event_t *>::iterator reply_recv_offset;
			int reply_recv_idx = i;
			
			reply_recv_offset = search_mig_msg(req_send_offset, &reply_recv_idx, mark_visit);
			if (reply_recv_offset == ev_list.end()) {
#if MSG_PATTERN_DEBUG
				cerr << "Check: a mig missing reply from kernel " << fixed << setprecision(1) << cur_msg->get_abstime() << endl;
#endif
			} else {
				mark_visit[i] = true;
				mark_visit[reply_recv_idx] = true;
				update_msg_pattern(dynamic_cast<msg_ev_t*>(*req_send_offset), dynamic_cast<msg_ev_t*>(*reply_recv_offset), NULL, NULL);
			}
			continue;
		}

		else if (header->get_remote_port() != 0 && header->get_local_port() != 0) {
			list<event_t *>::iterator req_send_offset = it;
			list<event_t *>::iterator req_recv_offset;
			list<event_t *>::iterator reply_send_offset;
			list<event_t *>::iterator reply_recv_offset;
			int req_send_idx = i;
			int req_recv_idx = req_send_idx;
			int reply_send_idx;
			int reply_recv_idx;
			
			uint32_t reply_sender_port_name = 0;
			uint32_t reply_recver_port_name = header->get_lport_name();
			pid_t request_pid = cur_msg->get_pid();
			pid_t reply_pid = -1;
		
			req_recv_offset = search_ipc_msg(
								&reply_sender_port_name,
								&reply_pid,
								header->get_remote_port(),
								header->get_local_port(),
								true,
								req_send_offset,
								&req_recv_idx,
								mark_visit, reply_recver_port_name);
			if (req_recv_offset == ev_list.end())
				continue;

			reply_send_idx = req_recv_idx;
			reply_send_offset = search_ipc_msg(
								&reply_sender_port_name,
								&reply_pid,
								header->get_local_port(),
								0,
								false,
								req_recv_offset,
								&reply_send_idx,
								mark_visit, reply_recver_port_name);
			if (reply_send_offset == ev_list.end()) {
				update_msg_pattern(dynamic_cast<msg_ev_t*>(*it), 
							dynamic_cast<msg_ev_t*>(*req_recv_offset),
							NULL, NULL);
				//TODO: checking if two sends connect to the same recv later
				continue;
			}

			reply_recv_idx = reply_send_idx;
			reply_recv_offset = search_ipc_msg(
								&reply_recver_port_name,
								&request_pid,
								header->get_local_port(),
								0,
								true,
								reply_send_offset,
								&reply_recv_idx,
								mark_visit, reply_recver_port_name);
			if (reply_recv_offset == ev_list.end()) { 
				continue;
			}

			mark_visit[req_send_idx] = true;
			mark_visit[req_recv_idx] = true;
			mark_visit[reply_send_idx] = true;
			mark_visit[reply_recv_idx] = true;
			update_msg_pattern(dynamic_cast<msg_ev_t*>(*it), 
							dynamic_cast<msg_ev_t*>(*req_recv_offset),
							dynamic_cast<msg_ev_t*>(*reply_send_offset),
							dynamic_cast<msg_ev_t*>(*reply_recv_offset));
		}

		else if (header->get_remote_port() != 0 && header->get_local_port() == 0) {
			list<event_t *>::iterator req_send_offset = it;
			list<event_t *>::iterator req_recv_offset;
			int req_send_idx = i;
			int req_recv_idx = req_send_idx;
			uint32_t reply_sender_port_name = 0;
			pid_t reply_pid = -1;

			req_recv_offset = search_ipc_msg(
								&reply_sender_port_name,
								&reply_pid,
								header->get_remote_port(),
								header->get_local_port(),
								true,
								req_send_offset,
								&req_recv_idx,
								mark_visit, 0);
			if (req_recv_offset == ev_list.end())
				continue;
			mark_visit[req_send_idx] = true;
			mark_visit[req_recv_idx] = true;
			update_msg_pattern(dynamic_cast<msg_ev_t*>(*it),
				dynamic_cast<msg_ev_t*>(*req_recv_offset), NULL, NULL);
		}
	}

#ifdef MSG_PATTERN_DEBUG
	cerr << "Total number of msg patterns:\n*************" << patterned_ipcs.size() << "**************" << endl;
#endif
	free(mark_visit);
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
list<event_t*>::iterator MsgPattern::search_ipc_msg(
			uint32_t * port_name, pid_t * pid,
			uint64_t remote_port, uint64_t local_port,
			bool is_recv,
			list<event_t*>::iterator begin_it,
			int * i,
			bool *mark_visit, uint32_t reply_recver_port_name)
{
	list<event_t*>::iterator cur_it = begin_it;

	for(cur_it++, *i = *i + 1; *i < ev_list.size(); ++cur_it, *i = *i + 1) {
		if (cur_it == ev_list.end())
			return cur_it;

		msg_ev_t * cur_ipc = dynamic_cast<msg_ev_t*>(*cur_it);
		msgh_t * header = cur_ipc->get_header();

		if (cur_ipc->is_freed_before_deliver() == true)
			continue;

		if (mark_visit[*i] == true || header->is_mig() == true)
			continue;

		if (header->check_recv() == is_recv
			&& header->get_remote_port() == remote_port
			&& header->get_local_port() == local_port) {
			/*checking processes*/
			if (*pid == -1) {
				*pid = cur_ipc->get_pid();
			} else {
				if (*pid != cur_ipc->get_pid()) {
#if MSG_PATTERN_DEBUG
					cerr << "Kernel Reply send/recv with incorrect pid " << fixed << setprecision(1) << cur_ipc->get_abstime();
					cerr << "\t" << fixed << setprecision(1) << (*begin_it)->get_abstime() << endl;
#endif
					continue;
				}
			}
			/*checking port names*/
			if (*port_name == 0) {
				assert(is_recv == true);
				*port_name = header->get_lport_name();
			} else {
				if (is_recv == false && header->is_from_kernel()) {
					if (header->get_rport_name() != reply_recver_port_name)	{
#if MSG_PATTERN_DEBUG
						cerr << "Kernel Reply send with incorrect port name " << fixed << setprecision(1) << cur_ipc->get_abstime();
						cerr << "\t" << fixed << setprecision(1) << (*begin_it)->get_abstime() << endl;
#endif
						continue;
					}
				} else { //recv == true || not from kernel
					if (*port_name != header->get_rport_name()) {
#if MSG_PATTERN_DEBUG
						cerr << "Reply Send with incorrect port name " << fixed << setprecision(1) << cur_ipc->get_abstime();
						cerr << "\t" << fixed << setprecision(1) << (*begin_it)->get_abstime() << endl;
#endif
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

list<event_t*>::iterator MsgPattern::search_mig_msg(list<event_t *>::iterator req_send_offset, 
								int *curr_idx, bool *mark_visit)
{
	msg_ev_t * mig_req = dynamic_cast<msg_ev_t*>(*req_send_offset);
	msgh_t *mig_req_h = mig_req->get_header();

	list<event_t *>::iterator curr_it =  req_send_offset;
	for (curr_it++, *curr_idx = *curr_idx + 1; 
				*curr_idx < ev_list.size();
				++curr_it, *curr_idx = *curr_idx +1) {
		msg_ev_t *cur_msg = dynamic_cast<msg_ev_t*>(*curr_it);
		msgh_t * curr_header = cur_msg->get_header();
		if (curr_header->is_mig() == false)
			continue;

		if (cur_msg->is_freed_before_deliver())
			cerr << "Wrong setting for free_before_deliver" << fixed << setprecision(1) << cur_msg->get_abstime() << endl;

		if (curr_header->get_remote_port() ==  mig_req_h->get_local_port()
			&& curr_header->get_rport_name() == mig_req_h->get_lport_name()
			&& mig_req->get_tid() == cur_msg->get_tid())
			return curr_it;
	}
	return curr_it;
}

list<msg_episode> & MsgPattern::get_patterned_ipcs(void)
{
	return patterned_ipcs;
}

list<msg_episode>::iterator MsgPattern::episode_of(msg_ev_t *msg_event)
{
	if (msg_event == NULL)
		return patterned_ipcs.end();

	list<msg_episode>::iterator it;
	for (it = patterned_ipcs.begin(); it != patterned_ipcs.end(); it++) {
		if ((*it).find(msg_event) != (*it).end())
			return it;
	}
	return it;
}

vector<msg_ev_t *> MsgPattern::sort_msg_episode(msg_episode & s)
{
	vector<msg_ev_t*> s_vector;
	s_vector.clear();
	set<msg_ev_t*>::iterator s_it;
	for (s_it = s.begin(); s_it != s.end(); s_it++) {
		s_vector.push_back(*s_it);
	}
	sort(s_vector.begin(), s_vector.end(), Parse::EventComparator::compare_time);
	return s_vector;
}

void MsgPattern::decode_patterned_ipcs(string & output_path)
{
	ofstream output(output_path, ofstream::out);
	list<msg_episode>::iterator it;
	int i = 0;
	output << "checking mach msg pattern ... " << endl;
	output << "number of patterns " << patterned_ipcs.size() << endl;
	for (it = patterned_ipcs.begin(); it != patterned_ipcs.end(); it++, i++) {
		output << "## " << i <<" size = " << (*it).size() << endl;
		vector<msg_ev_t *> cur = sort_msg_episode(*it);
		vector<msg_ev_t *>::iterator s_it;
		for (s_it = cur.begin(); s_it != cur.end(); s_it++) {
			(*s_it)->decode_event(0, output);
		}
	}
	output << "mach msg checking done" << endl;
	output.close();
}

void MsgPattern::verify_msg_pattern()
{
	
}
