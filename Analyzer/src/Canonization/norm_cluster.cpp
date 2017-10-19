#include "canonization.hpp"

NormCluster::NormCluster(cluster_t *c)
{
	norm_nodes.clear();
	elements.clear();
	vertexes.clear();
	vtid_map.clear();

	if (!(cluster = c))
		return;

	assign_virtual_tid();
	normalize_nodes(c->get_nodes());
	normalize_edges(c->get_edges());

	compressed = 0;
	//compress_cluster();
}

NormCluster::~NormCluster(void)
{
	vector<norm_group_t *>::iterator it;
	elements.clear();
	vertexes.clear();

	for (it = norm_nodes.begin(); it != norm_nodes.end(); it++) {
		assert(*it != NULL);
		delete *it;
	}
	norm_nodes.clear();
	vtid_map.clear();
}

uint64_t NormCluster::check_special_thread(group_t * cur_group, uint64_t spec_thread)
{
	if (spec_thread != MAIN_THREAD && spec_thread != NSEVENT_THREAD)
		return  -1;

	list<event_t *> & evlists = cur_group->get_container();
	list<event_t *>::iterator it;
	backtrace_ev_t * bt;

	for (it = evlists.begin(); it != evlists.end(); it++) {
		bt = (dynamic_cast<backtrace_ev_t *>(*it));
		if (bt != NULL) {
			string nsthread("_NSEventThread");
			string mainthread("NSApplication");
			if ((spec_thread == NSEVENT_THREAD && bt->check_backtrace_symbol(nsthread))
				|| (spec_thread == MAIN_THREAD && bt->check_backtrace_symbol(mainthread)))
				return bt->get_tid();
		}
	}
	return (uint64_t)-1;
}

void NormCluster::assign_virtual_tid(void)
{
	cluster->sort_nodes();
	vector<group_t *> nodes = cluster->get_nodes();
	vector<group_t *>::iterator it;
	group_t * cur_group;
	uint64_t tid;

	for (it = nodes.begin(); it != nodes.end(); it++) {
		cur_group = *it;
		tid = cur_group->get_group_id() >> GROUP_ID_BITS;

		if (vtid_map.find(tid) != vtid_map.end())
			continue;

		if (check_special_thread(cur_group, MAIN_THREAD) == tid)
			vtid_map[tid] = MAIN_THREAD;
		else if (check_special_thread(cur_group, NSEVENT_THREAD) == tid)
			vtid_map[tid] = NSEVENT_THREAD;
		else
			vtid_map[tid] = vtid_map.size() + 1;
	}
}

void NormCluster::normalize_nodes(vector<group_t*> &nodes)
{
	vector<group_t *>::iterator it;
	group_t * cur_group;
	norm_group_t * norm_group;
	uint64_t cur_group_vtid;

	// normalize from group to group
	for (it = nodes.begin(); it != nodes.end(); it++) {
		assert(*it);
		cur_group = *it;
		cur_group_vtid = vtid_map[cur_group->get_group_id() >> GROUP_ID_BITS];
		norm_group = new NormGroup(cur_group, cur_group_vtid);

		if (norm_group == NULL) {
			cerr << "OOM, unable to allocate memory for normalizing group " << endl;
			return;
		}

		if (norm_group->get_container().size()) {
			norm_nodes.push_back(norm_group);
			vertexes[cur_group] = norm_group;
			collect_elements_from_group(norm_group);
		} else {
			delete norm_group;
		}
		norm_group = NULL;
	}
}

void NormCluster::collect_elements_from_group(norm_group_t * norm_group) 
{
	list<norm_ev_t*> & normevents = norm_group->get_container();
	list<norm_ev_t*>::iterator it;

	for (it = normevents.begin(); it != normevents.end(); it++)
		elements[(*it)->get_real_event()] = *it;
}

void NormCluster::normalize_edges(vector<rel_t> & edges)
{
	vector<rel_t>::iterator it;

	for (it = edges.begin(); it != edges.end(); it++) {
		rel_t edge = *it;
		if (vertexes.find(edge.g_from) != vertexes.end())
			vertexes[edge.g_from]->add_out_edge();
		if (vertexes.find(edge.g_to) != vertexes.end())
			vertexes[edge.g_to]->add_in_edge();
	}
}

uint32_t NormCluster::check_group_sequence(vector<norm_group_t *>::iterator begin, int step, vector<norm_group_t *>::iterator end)
{
	vector<norm_group_t *>::iterator it = begin;
	advance(it, step);
	assert(distance(it, end) > 0);

	for (int i = 0; it != end; i++, it++) {
		if (**it != *(norm_nodes[i % step + distance(norm_nodes.begin(), begin)]))
			return 0;
	}

	return step;
}

void NormCluster::compress_cluster(void)
{
	/* check if the groups inside are generally identical
	 * especially for timer call create/cancel/out events
	 */
	if (norm_nodes.size() < 5)
		return;

	vector<norm_group_t *>::iterator it, begin, end;
	int step, check_begin = 1;

	#if 0
	for (begin = norm_nodes.begin(); check_begin >= 0 && begin != norm_nodes.end(); begin++, check_begin--) {
		end = norm_nodes.end();
		step = 2;
		it = begin;
		advance(it, step);
		if ((**it == **begin) && check_group_sequence(begin, step, end)) {
			compressed = (uint64_t)distance(norm_nodes.begin(), begin) << 32 | (uint64_t)step;
			break;	
		}
	}
	#endif

	const int size = norm_nodes.size() / 16 + 1;
	uint8_t *mark = (uint8_t *)malloc(sizeof(uint8_t) * size);

	if (mark == NULL) {
		cerr << "OOM, unable to alloc mark for compress" << endl;
		return;
	}

	for (begin = norm_nodes.begin(); check_begin >= 0 && begin != norm_nodes.end(); begin++, check_begin--) {
		memset(mark, 0, sizeof(uint8_t) * size);
		end = norm_nodes.end();
		for (step = distance(begin, end) / 2; step > 0;) {
			if (((mark[step / 8] >> (step % 8)) & 0x1) == 1)
				continue;

			it = begin;
			advance(it, step);

			if ((**it == **begin) && check_group_sequence(begin, step, end)) {
				compressed = (uint64_t)distance(norm_nodes.begin(), begin) << 32 | (uint64_t)step;
				step = step / 2;
				end = it;
				//break;	
			} else {
				// mark any smaller step that can be divided by curstep as not pressible factor
				for (int i = pow(step, 0.5); i > 0; i--) {
					if (step % i == 0) {
						mark[i / 8] |= (1 << (i % 8));
						mark[(step / i) / 8] |= (1 << ((step / i) % 8));
					}
				}
				step--;
			}
		}

		if (compressed != 0)
			break;
	}
	free(mark);
}

void NormCluster::topological_sort(void)
{
	
}

void NormCluster::check_compress(void)
{
	if (compressed) {
		cerr << "[Cluster compress] Cluster_id " << hex << get_cluster_id() << " compress with step " << (uint32_t)compressed;
		cerr << " begins at " << hex << norm_nodes[compressed >> 32]->get_group_id() << endl;
	}

	vector<norm_group_t *>::iterator it;
	for (it = norm_nodes.begin(); it != norm_nodes.end(); it++) {
		(*it)->check_compress();
	}
}

bool NormCluster::operator==(NormCluster &other)
{
	/* compare groups by threads: main_thread / nsevent_thread / others
	 * compare others by procs
	 */
	vector<norm_group_t*> other_nodes = other.get_norm_nodes();
	vector<norm_group_t*>::iterator other_it;
	vector<norm_group_t*>::iterator this_it, end = norm_nodes.end();
	bool ret = true;
	
	/* TODO number of groups is misleading. 
	 * groups with events repeated periodically
	 * may of different repeating times
	 */
	
	#if 0
	if (compressed) {
		if (other.is_compressed() == false) {
			return false;
		}
		this_it = norm_nodes.begin();
		advance(this_it, uint32_t(compressed >> 32) + (uint32_t)compressed);
		end = this_it;
	} else {
		if (other_nodes.size() != norm_nodes.size()) {
			return false;
		}
	}
	#endif

	uint64_t norm_num = 0;
	for (this_it = norm_nodes.begin(); this_it != norm_nodes.end(); this_it++)
		if ((*this_it)->check_norm() == true)
			norm_num++;

	if (norm_num == norm_nodes.size())
		return true;
	return false;

	#if 0
	for (other_it = other_nodes.begin(), this_it = norm_nodes.begin();
		other_it != other_nodes.end() && this_it != end;
		other_it++, this_it++) {
		if (**this_it != **other_it) {
			return false;
		}
	}
	
	for (; this_it != norm_nodes.end(); this_it++) {
		if ((*this_it)->check_norm() == false)
			return false;
	}

	return ret;
	#endif
}

bool NormCluster::operator!=(NormCluster & other)
{
	return !(*this == other);
}

void NormCluster::decode_cluster(ofstream &output)
{
	//cluster->decode_cluster(output);
	//cluster->streamout_cluster(output);

	vector<norm_group_t *>::iterator it;
	for (it = norm_nodes.begin(); it != norm_nodes.end(); it++) {
		if ((*it)->check_norm() == false)
			(*it)->decode_group(output);
	}
}
