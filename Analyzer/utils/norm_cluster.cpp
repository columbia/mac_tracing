#include "norm_cluster.hpp"

#define DEBUG_NORMCLUSTER 0

NormCluster::NormCluster(cluster_t *c, map<uint64_t, bool> &_key_events)
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
	vector<norm_group_t *>::iterator it;
	for (it = sort_vertexes.begin(); it != sort_vertexes.end(); it++) {
		delete(*it);
	}
	sort_vertexes.clear();
	cluster = NULL;
}

void NormCluster::init_vertex_degrees(vector<rel_t> &edges)
{
	vector<norm_group_t *>::iterator ng_it;
	for (ng_it = sort_vertexes.begin(); ng_it != sort_vertexes.end(); ng_it++) {
		(*ng_it)->clear_degrees();
	}

	vector<rel_t>::iterator it;
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

void NormCluster::create_vertexes(vector<group_t *> &nodes)
{
	vector<group_t *>::iterator it;
	group_t *cur_group;
	norm_group_t *norm_group;
	
	// normalize from group to group
	for (it = nodes.begin(); it != nodes.end(); it++) {
		assert(*it);
		cur_group = *it;
		norm_group = new NormGroup(cur_group, key_events);
		if (!norm_group) {
#if DEBUG_NORMCLUSTER 
			mtx.lock();
			cerr << "OOM, unable to allocate memory for normalizing group " << endl;
			mtx.unlock();
#endif
			exit(EXIT_FAILURE);
		}
		vertexes_map[cur_group] = norm_group;
		vertexes_reverse_map[norm_group] = cur_group;
		sort_vertexes.push_back(norm_group);
	}
}

bool NormCluster::compare_vertexes_in(norm_group_t *elem1, norm_group_t *elem2)
{
	return elem1->get_in_edges() != elem2->get_in_edges() ? elem1->get_in_edges() < elem2->get_in_edges() : \
									elem1->original_size() < elem2->original_size();
}

norm_group_t *NormCluster::sort_vertexes_in(vector<norm_group_t *>::iterator begin, vector<norm_group_t *>::iterator end)
{
	assert(begin != end);
	sort(begin, end, &NormCluster::compare_vertexes_in);
#if DEBUG_NORMCLUSTER
	mtx.lock();
	cerr << "Least in Node is " << hex << (*begin)->get_group_id();
	cerr << "\tIn edges: " << dec << (*begin)->get_in_edges() << endl;
	vector<norm_group_t *>::iterator it = begin;
	for (; it != end; it++)
		cerr << dec << (*it)->get_in_edges() << "\t";
	cerr << endl;
	mtx.unlock();
#endif
	return *begin;
}

void NormCluster::remove_degree_edges_from(norm_group_t *group, node_edges_map_t &group_inedges_map, node_edges_map_t &group_outedges_map)
{
	if (!group) {
#if DEBUG_NORMCLUSTER
		mtx.lock();
		cerr << "No group to remove" << endl;
		mtx.unlock();
#endif
		return;
	}
	node_edges_map_t::iterator it;
	auto range = group_outedges_map.equal_range(vertexes_reverse_map[group]);
#if DEBUG_NORMCLUSTER
	mtx.lock();
	cerr << "Remove in edges from " << distance(range.first, range.second) << "nodes"<< endl;
	mtx.unlock();
#endif

	for (it = range.first; it != range.second; it++) {
		rel_t edge = it->second;
		vertexes_map[edge.g_to]->dec_in_edge();
	}
}

void NormCluster::topological_sort(void)
{
	vector<norm_group_t *>::iterator begin = sort_vertexes.begin(), end = sort_vertexes.end();
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
	vector<norm_group_t *> other_vertexes = other.get_sort_vertexes();
	vector<norm_group_t *>::iterator other_it, other_end = other_vertexes.end();
	vector<norm_group_t *>::iterator this_it, this_end = sort_vertexes.end();
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

void NormCluster::decode_cluster(ofstream &output)
{
	vector<norm_group_t *>::iterator it;
	for (it = sort_vertexes.begin(); it != sort_vertexes.end(); it++) {
		(*it)->decode_group(output);
	}
}
