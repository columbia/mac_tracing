#include "voucher_connection.hpp"
#include "mach_msg.hpp"

IPCForest::IPCForest(list<event_t *> & _evlist)
:evlist(_evlist)
{
}

void IPCForest::construct_new_node(voucher_ev_t * voucher_info)
{
	uint64_t cur_tid = voucher_info->get_tid();
	
	if (incomplete_nodes.find(cur_tid) != incomplete_nodes.end()) {
		mtx.lock();
		cerr << "Error : Missing ipc msg to carry voucher at ";
		cerr << fixed << setprecision(1) << incomplete_nodes[cur_tid]->get_voucher()->get_abstime();
		cerr << "\tthread_id = " << hex << cur_tid << endl;
		mtx.unlock();
		delete(incomplete_nodes[cur_tid]);
	}
	
	if (voucher_info->get_carrier()->is_freed_before_deliver() == false) {
		IPCNode * new_node = new IPCNode(voucher_info);
		incomplete_nodes[cur_tid] = new_node;
	}
}

bool IPCForest::update_voucher_and_connect_ipc(IPCNode *ipcnode)
{
	voucher_ev_t * voucher_info = ipcnode->get_voucher();
	uint64_t vchaddr = voucher_info->get_voucher_addr();
	uint8_t cur_status = ipcnode->get_msg()->get_header()->check_recv() ? HOLD_RECV : HOLD_SEND;
	struct voucher_state vs = {.holder = voucher_info->get_tid(),
							   .status = cur_status,
							   .guarder = ipcnode};
	bool ret = false;
	
	if (voucher_status.find(vchaddr) != voucher_status.end()) {
		if (voucher_status[vchaddr].guarder == NULL) {
			mtx.lock();
			cerr << "Error : prev voucher status maintained without corresponding ipc activity ";
			cerr << fixed << setprecision(1) << voucher_info->get_abstime() << endl;
			mtx.unlock();
			goto out;
		}

		// check the relationship withe prev guard node;
		IPCNode *prev = voucher_status[vchaddr].guarder;
		uint8_t prev_guard_status = prev->get_msg()->get_header()->check_recv() ? HOLD_RECV : HOLD_SEND;
		switch (prev_guard_status) {
			case HOLD_SEND:
				if (cur_status == HOLD_RECV) {
					// SEND -> RECV add as child
					//TODO : check msg_pattern
					prev->add_child(ipcnode);
					ret = true;
				} else {
					assert(cur_status == HOLD_SEND);
					// SEND -> SEND add as sibling
					// Hint : from the log, they are referred by differnt threads;
					if (prev->get_parent()) {
						prev->get_parent()->add_child(ipcnode);
						ret = true;
					}
				}
				break;

			case HOLD_RECV:
				if (cur_status == HOLD_RECV) {
					// RECV -> RECV
					// add as sibling
					if (prev->get_parent()) {
						prev->get_parent()->add_child(ipcnode);
						ret = true;
					}
				} else {
 					assert(cur_status == HOLD_SEND);
					// RECV -> SEND
					// TODO :check msg_pattern
					// add as child
					prev->add_child(ipcnode);
					ret = true;
				}
				break;

			default:
				mtx.lock();
				cerr << "Error: unkown voucher status set by ";
				cerr << fixed << setprecision(1) << prev->get_voucher()->get_abstime() << endl;
				mtx.unlock();
				
		}
	}
out:
	// update voucher_status
	voucher_status[vchaddr] = vs;
	return ret;
}

void IPCForest::construct_ipc_relation(msg_ev_t * mach_msg)
{
	uint64_t cur_tid =  mach_msg->get_tid();
	if (incomplete_nodes.find(cur_tid) == incomplete_nodes.end()) {
		#if DEBUG_VOUCHER
		mtx.lock();
		cerr << "Check : msg without voucher ";
		cerr  << fixed << setprecision(1) << mach_msg->get_abstime() << endl;
		mtx.unlock();
		#endif
		return;
	}

	IPCNode * ipc_node = incomplete_nodes[cur_tid];
	if (ipc_node->get_voucher()->get_carrier() == mach_msg)
		ipc_node->set_msg(mach_msg);
	else {
		mtx.lock();
		cerr << "Error: Incorrect voucher_msg pair" << endl;
		mtx.unlock();
	}

	bool add_to_tree = update_voucher_and_connect_ipc(ipc_node);

	if (add_to_tree ==  false) {
		IPCTree * ipctree = new IPCTree(ipc_node);
		Roots.push_back(ipctree);
	}
	incomplete_nodes.erase(cur_tid);
}

void IPCForest::update_voucher_status(voucher_conn_ev_t * voucher_copy)
{
	uint64_t cur_tid =  voucher_copy->get_tid(); 
	uint64_t voucher_ori = voucher_copy->get_voucher_ori();
	uint64_t voucher_new = voucher_copy->get_voucher_new();

	if (voucher_status.find(voucher_ori) == voucher_status.end()) {
		mtx.lock();
		cerr << "Check: No corresponding original voucher found ";
		cerr << fixed << setprecision(1) << voucher_copy->get_abstime() << endl;
		mtx.unlock();
		return;
	}

	if (voucher_status.find(voucher_new) != voucher_status.end()) {
		if (voucher_status[voucher_new].status != IN_COPY
			|| (uint64_t)(voucher_status[voucher_new].holder) != cur_tid) {
			mtx.lock();
			cerr << "Error: try to copy to a inuse voucher ";
			cerr << fixed << setprecision(1) << voucher_copy->get_abstime() << endl;
			mtx.unlock();
		}

		if (voucher_status[voucher_new].status == IN_COPY
			&& voucher_status[voucher_new].voucher_copy_src != voucher_ori) {
			mtx.lock();
			cerr << "Check: copy voucher from different voucher src:";
			cerr << fixed << setprecision(1) << voucher_copy->get_abstime() << endl;
			mtx.unlock();
			//There must be only one bank attr copied
		}// otherwise ignore this copy
	} else {
		// get src_voucher corresponding ipcnode from voucher_status map
		// insert the new voucher, copy the guard to the new voucher
		IPCNode * ori_node = voucher_status[voucher_ori].guarder;
		struct voucher_state vs = { .holder = cur_tid,
								.status = IN_COPY,
								.guarder = ori_node,
								.voucher_copy_src = voucher_ori};
		voucher_status[voucher_new] = vs;
	}
}
void IPCForest::update_voucher_status(voucher_transit_ev_t *voucher_transit)
{
	uint64_t cur_tid = voucher_transit->get_tid();
	uint64_t voucher_dst = voucher_transit->get_voucher_dst(); 
	uint64_t voucher_src = voucher_transit->get_voucher_src();
	switch(voucher_transit->get_type()) {
		case CREATE:
			//if create : use voucher_dst after the conn update voucher status of new voucher
			//			  update the status of the new voucher
			//
			if (voucher_status.find(voucher_dst) == voucher_status.end()) {
#if DEBUG_VOUCHER
				mtx.lock();
				cerr << "Check : create voucher without attr copy ";
				cerr << fixed << setprecision(1) << voucher_transit->get_abstime() << endl;
				mtx.unlock();
#endif
			} else {
				if (voucher_status[voucher_dst].holder != cur_tid) {
#if DEBUG_VOUCHER
					mtx.lock();
					cerr << "Check : create voucher other thread held ";
					cerr << fixed << setprecision(1) << voucher_transit->get_abstime() << endl;
					mtx.unlock();
#endif
				} else {
					voucher_status[voucher_dst].status = IN_CREATE;
				}
			}
			break;

		case REUSE:
			//if reuse : use voucher_src after the conn update voucher status of new voucher
			//            update the status of the reused voucher, leave deletion of new voucher
			//            to voucher_terminate events
			if (voucher_status.find(voucher_src) == voucher_status.end()) {
#if DEBUG_VOUCHER
				mtx.lock();
				cerr << "Check : reuse voucher without attr copy procedure ";
				cerr << fixed << setprecision(1) << voucher_transit->get_abstime() << endl;
				mtx.unlock();
#endif
			} else {
				if (voucher_status.find(voucher_dst) != voucher_status.end()) {
#if DEBUG_VOUCHER
					mtx.lock();
					cerr << "Check : reuse voucher held by other ipc_node ";
					cerr << fixed << setprecision(1) << voucher_transit->get_abstime() << endl;
					mtx.unlock();
#endif
				}
				voucher_status[voucher_dst].holder = voucher_status[voucher_src].holder;
				voucher_status[voucher_dst].status = IN_REUSE;
				voucher_status[voucher_dst].guarder = voucher_status[voucher_src].guarder;
			}
			break;

		default:
			break;
	}
}

void IPCForest::construct_forest(void)
{
	list<event_t *>::iterator it;
	voucher_ev_t * voucher_info;
	msg_ev_t * mach_msg;
	voucher_conn_ev_t * voucher_copy;
	voucher_transit_ev_t * voucher_transit;
	voucher_dealloc_ev_t * voucher_terminate;

	for (it = evlist.begin(); it != evlist.end(); it++) {
		voucher_info = dynamic_cast<voucher_ev_t *>(*it);
		if (voucher_info) {
			construct_new_node(voucher_info);
		}
		
		mach_msg = dynamic_cast<msg_ev_t *>(*it);
		if (mach_msg) {
			construct_ipc_relation(mach_msg);
		}
		
		voucher_copy = dynamic_cast<voucher_conn_ev_t *>(*it);
		if (voucher_copy) {
			update_voucher_status(voucher_copy);
		}
		
		voucher_transit = dynamic_cast<voucher_transit_ev_t *>(*it);
		if (voucher_transit) {
			update_voucher_status(voucher_transit);
		}
		
		voucher_terminate = dynamic_cast<voucher_dealloc_ev_t *>(*it);
		if (voucher_terminate) {
			//update voucher, delete it from map
			uint64_t voucher = voucher_terminate->get_voucher();
			voucher_status.erase(voucher);
		}
	}
}

void IPCForest::check_remain_incomplete_nodes(void)
{
	if (incomplete_nodes.size()) {
		// clear the rest node
		// print debug info for checking
		map<uint64_t, IPCNode *>::iterator it;
		for (it = incomplete_nodes.begin(); it != incomplete_nodes.end(); it++) {
			mtx.lock();
			cerr << "Incomplete IPCNode at " << fixed << setprecision(1) << (it->second)->get_voucher()->get_abstime() << endl;
			mtx.unlock();
			delete(it->second);
		}
	}
}

void IPCForest::decode_voucher_relations(const char * outfile)
{
	vector<IPCTree *>::iterator it;
	uint64_t tree_index = 0;
	ofstream output(outfile);
	if (output.fail()) {
		mtx.lock();
		cerr << "unable to open file" << outfile << endl;
		mtx.unlock();
		return;
	}

	for (it = Roots.begin(); it !=  Roots.end(); it++) {
		IPCTree * tree = *it;
		cout << "Decode Tree " << dec << tree_index << endl;
		output << "#Tree " << dec << tree_index << endl;
		tree->decode_tree(tree->get_root(), output);
		tree_index++;
	}
	output.close();
}

void IPCTree::decode_tree(IPCNode *root, ofstream & output)
{
	uint32_t indent = root->get_depth();
	output << dec << indent << "\t";

	/*
	while(indent != 0) {
		output << " ";
		indent--;
	}
	*/

	root->get_voucher()->streamout_event(output);

	vector<IPCNode *> children = root->get_children();
	vector<IPCNode *>::iterator it;
	for (it = children.begin(); it != children.end(); it++) {
		decode_tree(*it, output);
	}
}
