#include "cluster_filter.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

Filter::Filter(clusters_t * clusters)
{
	select_clusters(clusters);
}

Filter::~Filter()
{
	map<uint64_t, cluster_t *>::iterator it;
	for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
		if (it->second != NULL) {
			delete(it->second);
		}
	}
}

void Filter::select_clusters(clusters_t * clusters)
{
	original_clusters.clear();
	map<uint64_t, cluster_t *> cluster_map = clusters->get_clusters();
	map<uint64_t, cluster_t *>::iterator it;
	cluster_t * cur_cluster;

	for (it = cluster_map.begin(); it != cluster_map.end(); it++) {
		cur_cluster = it->second;
		if (cur_cluster->check_ground() == true) {
			original_clusters[it->first] = cur_cluster;
		}
	}
}

void Filter::sort_cluster_edges(vector<rel_t> &cluster_edges, node_edges_map_t & to_edges, node_edges_map_t & from_edges)
{
	vector<rel_t>::iterator it;
	to_edges.clear();
	from_edges.clear();
	for (it = cluster_edges.begin(); it != cluster_edges.end(); it++) {
		rel_t edge = *it;
		to_edges.insert(make_pair(edge.g_to, edge));
		from_edges.insert(make_pair(edge.g_from, edge));
	}
}

int Filter::get_node_idx_in_cluster(group_t * node, cluster_t * cluster)
{
	vector<group_t *> & cluster_nodes = cluster->get_nodes();
	vector<group_t *>::iterator pos = find(cluster_nodes.begin(), cluster_nodes.end(), node);
	int ret = -1;

	if (pos != cluster_nodes.end())
		ret = pos - cluster_nodes.begin();

	return ret;
}

int Filter::back_traverse_from(group_t *infected_group, bool * visited, cluster_t * cur_cluster,
				node_edges_map_t &to_edges, cluster_t * dst_cluster)
{
	assert(visited && infected_group);
	int idx = get_node_idx_in_cluster(infected_group, cur_cluster);

	assert(idx >= 0);
	if (visited[idx] == true || idx == 0)
		return 0;
	visited[idx] = true;

	multimap_range_t range = to_edges.equal_range(infected_group);
	if (range.first == range.second)
		return 0;

	//cerr << "edge size = " << distance(range.first, range.second) << endl;
	node_edges_map_t::iterator it;
	rel_t edge;
	group_t * g_from;

	for (it = range.first; it != range.second; it++) {
		edge = it->second;
		dst_cluster->add_edge(edge.g_from, edge.g_to, edge.e_from, edge.e_to, edge.rel_type);
		g_from = edge.g_from;
		if (g_from == infected_group)
			continue;
		if (get_node_idx_in_cluster(g_from, dst_cluster) != -1) //node is already in the cluster
			continue;
		//cerr << "Traverse through node " << hex << g_from->get_group_id() << " from Cluster " << hex << cur_cluster->get_cluster_id() << endl;
		dst_cluster->add_node(g_from);
		back_traverse_from(g_from, visited, cur_cluster, to_edges, dst_cluster);
	}
	return 0;
}

cluster_t * Filter::filter_by_traverse(int index, cluster_t *cur_cluster)
{
	vector<group_t*> groups = cur_cluster->get_gt_groups();
	vector<group_t*>::iterator it;
	group_t * infected_group;
	node_edges_map_t to_edges, from_edges;
	int size = cur_cluster->get_nodes().size();
	bool *visited = (bool *)malloc(sizeof(bool) * size);

	if (visited == NULL) {
		cerr << "Error : OOM" << endl;
		return NULL;
	}
	memset(visited, 0, sizeof(bool) * size);
	sort_cluster_edges(cur_cluster->get_edges(), to_edges, from_edges);

	cluster_t *dst_cluster = new Cluster();
	if (dst_cluster == NULL) {
		cerr << "Error : OOM" << endl;
		return NULL;
	}
	dst_cluster->set_cluster_id(cur_cluster->get_cluster_id());

	for (it = groups.begin(); it != groups.end(); it++) {
		//cerr << "Check ground node " << hex << (*it)->get_group_id() << " from Cluster " << hex << cur_cluster->get_cluster_id() << endl;
		if (get_node_idx_in_cluster(*it, dst_cluster) != -1)
			continue;
		infected_group = *it;
		//cerr << "Add node " << hex << infected_group->get_group_id() << " from Cluster " << hex << cur_cluster->get_cluster_id() << endl;
		dst_cluster->add_node(infected_group);
 		back_traverse_from(infected_group, visited, cur_cluster, to_edges, dst_cluster);
	}
	//TODO : check if may miss edges!!

	free(visited);
	filtered_clusters[index] = dst_cluster;
	return dst_cluster;
}

bool Filter::read_processes(string filename)
{
	ifstream input(filename);
	if (input.fail()) {
		cerr << "No file found to read process intersection" << endl;
		return false;
	}

	string line;
	while (getline(input, line) && line.size() > 0) {
		process_set.insert(line);
	}
	//cerr << "Intersection size = " << process_set.size() << endl;
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
		
	multimap<group_t *, rel_t>::iterator edge_it;
	for (edge_it = range.first; edge_it != range.second; edge_it++) {
		if (edge_it->second == edge) {
			edges.erase(edge_it);
			return true;
		}
	}
	return false;
}

bool Filter::audit_deletion_of_node(group_t *cur_node, node_edges_map_t &from_edges, node_edges_map_t &to_edges)
{
	/* check if need propagate the removing to
	 * the node edge.g_from
	 * if neither from edges nor to edges with other nodes
	 * delete it
	 */
	multimap_range_t range = to_edges.equal_range(cur_node);
	node_edges_map_t::iterator edge_it;
	rel_t edge;
	int range_distance = distance(range.first, range.second);
	
	if (range_distance > 0) {
		for (edge_it = range.first; edge_it != range.second; edge_it++) {
			edge = edge_it->second;
			if (edge.g_from != cur_node) {
				return false;
			}
		}
	}
	/* No other nodes that have edges to the cur node here */
	range = from_edges.equal_range(cur_node);
	range_distance = distance(range.first, range.second);
	if (range_distance > 0) {
		for (edge_it = range.first; edge_it != range.second; edge_it++) {
			edge = edge_it->second;
			if (edge.g_to != cur_node) {
				return false;
			}
		}
	}
	return true;
}

bool Filter::audit_deletion_of_node(group_t *cur_node, node_edges_map_t &to_edges)
{
	/* check if need propagate the removing to
	 * the node edge.g_to
	 * if there is not from edges any more with other nodes
	 * delete it
	 */
	multimap_range_t range = to_edges.equal_range(cur_node);
	node_edges_map_t::iterator edge_it;
	rel_t edge;
	int range_distance = distance(range.first, range.second);
	
	if (range_distance > 0) {
		for (edge_it = range.first; edge_it != range.second; edge_it++) {
			edge = edge_it->second;
			if (edge.g_from != cur_node) {
				return false;
			}
		}
	}
	return true;
}

void Filter::remove_node(cluster_t * cur_cluster, group_t * cur_node, node_edges_map_t &to_edges, node_edges_map_t &from_edges)
{
	vector<group_t *> propagate_deletion_nodes;
	propagate_deletion_nodes.clear();
	/* processing all edges points to cur_node (node_A)
	 * remove the edge from the cluster
	 * do update on from_edges_map and to_edges_map
	 *   if the edge connected from node_B
	 *   and remove the particular edge in the node_B's from_edges_map
	 * remove all the edges points to cur_node
	 */
	multimap_range_t to_range = to_edges.equal_range(cur_node);
	if (to_range.first != to_range.second) {
		multimap<group_t *, rel_t>::iterator edge_it;
		for (edge_it = to_range.first; edge_it != to_range.second; edge_it++) {
			rel_t edge = edge_it->second;
			cur_cluster->remove_edge(edge);

			if (edge.g_from != edge.g_to) {
				auto peer_from_range = from_edges.equal_range(edge.g_from);
				erase_edge_in_range(edge, peer_from_range, from_edges);
				/* check if need propagate the removing to
				 * the node edge.g_from
				 * if neither from edges nor to edges with other nodes
				 * delete it
				 */
				if (audit_deletion_of_node(edge.g_from, from_edges, to_edges))
					propagate_deletion_nodes.push_back(edge.g_from);
			}
		}
		to_edges.erase(to_range.first, to_range.second);
	}

	/* processing all edges from cur_node (node_A) 
	 * remove the edge from the cluster
	 * do update on from_edges_map and to_edges_map
	 *   if the edge connected to node_B
	 *   remove the particular edge in the node_B's to_edges_map
	 * remove all the edges from cur_node
	 */
	multimap_range_t from_range = from_edges.equal_range(cur_node);
	if (from_range.first != from_range.second) {
		multimap<group_t *, rel_t>::iterator edge_it;
		for (edge_it = from_range.first; edge_it != from_range.second; edge_it++) {
			rel_t edge = edge_it->second;
			cur_cluster->remove_edge(edge);

			if (edge.g_from != edge.g_to) {
				auto peer_to_range = to_edges.equal_range(edge.g_to);
				erase_edge_in_range(edge, peer_to_range, to_edges);
				/* check if need propagate the removing to
				 * the node edge.g_to
				 * if there is not from edges any more with other nodes
				 * delete it
				 */
				if (audit_deletion_of_node(edge.g_to, to_edges))
					propagate_deletion_nodes.push_back(edge.g_to);
			}
		}
		from_edges.erase(from_range.first, from_range.second);
	}

	//remove current node
	cerr <<  "Delete " << hex << cur_node->get_group_id() << " with size = " << hex <<  cur_node->get_size() << endl;
	cur_cluster->remove_node(cur_node);

	vector<group_t *> ::iterator node_it;
	for (node_it = propagate_deletion_nodes.begin();
			node_it != propagate_deletion_nodes.end(); node_it++) {
		remove_node(cur_cluster, *node_it, to_edges, from_edges);
	}
}

cluster_t * Filter::filter_by_proc_intersection(cluster_t *cluster)
{
	if (process_set.size() == 0)
		return cluster;

	node_edges_map_t to_edges, from_edges;
	sort_cluster_edges(cluster->get_edges(), to_edges, from_edges);
	vector<group_t *> nodes = cluster->get_nodes();
	vector<group_t *>::iterator it;

	for (it = nodes.begin(); it != nodes.end(); it++) {
		group_t *cur_node =  *it;
		string proc_name(cur_node->get_first_event()->get_procname());
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

	map<uint64_t, cluster_t*>::iterator it;

	switch (type) {
		case PATH_FILTER: {
			for (it = original_clusters.begin(); it != original_clusters.end(); it++)
				filtered_clusters[it->first] = NULL;
			for (it = original_clusters.begin(); it != original_clusters.end(); it++) {
				filter_by_traverse(it->first, it->second);
				//ioService.post(boost::bind(&Filter::filter_by_traverse, this, it->first, it->second));
				//OOM
			}
			break;
		}
		case PROCESS_FILTER: {
			if (read_processes("process_intersection.log") == false)
				break;

			if (filtered_clusters.size()) {
				for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++)
					if (it->second != NULL) {
						ioService.post(boost::bind(&Filter::filter_by_proc_intersection, this, it->second));
					}
			} else {
				for (it = original_clusters.begin(); it != original_clusters.end(); it++)
					ioService.post(boost::bind(&Filter::filter_by_proc_intersection, this, it->second));
			}
			break;
		}
		default:
			break;
	}
	work.reset();
	threadpool.join_all();
	cerr << "Finished Filtering " << type << endl;
	//ioService.stop();
}

void Filter::clusters_filter_para()
{
	clusters_filter_para(PATH_FILTER);
	//clusters_filter_para(PROCESS_FILTER);
}

void Filter::streamout_buggy_clusters(string output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *>::iterator it;

	#ifdef PATH_FILTER
	for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
	#else
	for (it =  original_clusters.begin(); it != original_clusters.end(); it++) {
	#endif
		cluster_t * cur_cluster = it->second;
		if (cur_cluster->check_infected() == true) {
			output << "#Cluster " << hex << cur_cluster->get_cluster_id() << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
			cur_cluster->streamout_cluster(output);
		}
	}
	output.close();
}

void Filter::streamout_filtered_clusters(string output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *>::iterator it;

	#ifdef PATH_FILTER
	for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
	#else
	for (it =  original_clusters.begin(); it != original_clusters.end(); it++) {
	#endif
		cluster_t * cur_cluster = it->second;
		output << "#Cluster " << hex << cur_cluster->get_cluster_id() << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->streamout_cluster(output);
	}
	output.close();
}

void Filter::decode_filtered_clusters(string output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *>::iterator it;

	#ifdef PATH_FILTER
	for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
	#else
	for (it =  original_clusters.begin(); it != original_clusters.end(); it++) {
	#endif
		cluster_t * cur_cluster = it->second;
		output << "Cluster_" << cur_cluster->get_cluster_id() << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->streamout_cluster(output);
	}
}

void Filter::decode_edges(string output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *>::iterator it;

	#ifdef PATH_FILTER
	for (it = filtered_clusters.begin(); it != filtered_clusters.end(); it++) {
	#else
	for (it =  original_clusters.begin(); it != original_clusters.end(); it++) {
	#endif
		cluster_t * cur_cluster = it->second;
		output << "Cluster_" << cur_cluster->get_cluster_id() << "(num_of_groups = " << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->decode_edges(output);
	}
}
