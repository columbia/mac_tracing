#include "canonization.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Normalizer::Normalizer(map<uint64_t, cluster_t *> & clusters)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &ioService));

	cerr << "Total number of clusters " << hex << clusters.size() << endl;
	map<uint64_t, cluster_t *>::iterator it;
	for (it = clusters.begin(); it != clusters.end(); it++)
		norm_clusters[it->first] = (norm_cluster_t*)NULL;
	
	for (it = clusters.begin(); it != clusters.end(); it++)
		ioService.post(boost::bind(&Normalizer::normalize_cluster, this, it->first, it->second));

	work.reset();
	threadpool.join_all();
	//ioService.stop();

	map<uint64_t, norm_cluster_t *>::iterator norm_it;
	for (norm_it = norm_clusters.begin(); norm_it != norm_clusters.end();) {
		if (norm_it->second == NULL)
			norm_it = norm_clusters.erase(norm_it);
		else
			norm_it++;
	}
	cerr << "Normalized " << hex << norm_clusters.size() << " clusters" << endl;
	//check_compress();
}

void Normalizer::normalize_cluster(uint64_t index, cluster_t * cluster)
{
	norm_cluster_t * new_cluster = new NormCluster(cluster);

	if (!new_cluster) {
		cerr << "OOM, unable to allocate memory for normalize cluster" << endl;
		return;
	}

	if ((new_cluster->get_norm_nodes()).size())
		norm_clusters[index] = new_cluster;
	else
		delete new_cluster;
}

void Normalizer::check_compress(void)
{
	map<uint64_t, norm_cluster_t *>::iterator it;
	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++) {
		it->second->check_compress();
	}
}

Normalizer::~Normalizer(void)
{
	map<uint64_t, norm_cluster_t*>::iterator it;
	complex_clusters.clear();
	outstanders.clear();

	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++) {
		assert(it->second != NULL);
		delete(it->second);
	}
	norm_clusters.clear();
}

void Normalizer::divide_by_complexity(void)
{
	map<uint64_t, norm_cluster_t*>::iterator it;
	norm_cluster_t * cur_cluster;
	uint64_t complexity;

	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++) {
		cur_cluster = it->second;
		complexity = cur_cluster->complexity();
		if (complex_clusters.find(complexity) == complex_clusters.end()) {
			list<norm_cluster_t *> l;
			complex_clusters[complexity] = l;
		}
		complex_clusters[complexity].push_back(cur_cluster);
	}
}

void Normalizer::check_by_complexity(list<norm_cluster_t *> & cluster_list, ofstream  & output)
{
	output << "#Number of clusters with the same complexity " << hex << cluster_list.size() << endl;
	list<norm_cluster_t *>::iterator it;
	for (it = cluster_list.begin(); it != cluster_list.end(); it++) {
		output << "#Cluster " << hex << (*it)->get_cluster_id() << "\t(num of groups = " << hex << ((*it)->get_norm_nodes()).size() << ")\n";	
		(*it)->decode_cluster(output);
	}
}

void Normalizer::inner_compare(const char * output_file)
{
	ofstream output(output_file);
	if (output.fail()) {
		cerr << "Error : fail to open file " << output_file << endl;
		return;
	}

	map<uint64_t, list<norm_cluster_t *> >::iterator it;
	
	output << "#Complexity Numbers = " << complex_clusters.size() << endl;
	for (it = complex_clusters.begin(); it != complex_clusters.end(); it++) {
		output << "#Complexity " << hex << it->first << endl; 
		check_by_complexity(it->second, output);
	}
	output.close();
}

void Normalizer::compare(Normalizer * base, const char *output_file)
{
	ofstream output(output_file);
	if (output.fail()) {
		cerr << "Error : fail to open file " << output_file << endl;
		return;
	}

	map<uint64_t, norm_cluster_t *> &base_clusters = base->get_normed_clusters();
	map<uint64_t, norm_cluster_t *>::iterator base_it;
	map<uint64_t, norm_cluster_t *>::iterator cur_it;

	uint32_t equal = 0;
	for (cur_it = norm_clusters.begin(); cur_it != norm_clusters.end(); cur_it++) {
		bool found =  false;
		norm_cluster_t * cur_cluster = cur_it->second;
		for (base_it = base_clusters.begin(); base_it != base_clusters.end(); base_it++) {
			norm_cluster_t *base_cluster = base_it->second;
			
			if (*cur_cluster == *base_cluster) {
				if (cur_cluster->is_ground()) {
					output << "cluster " << hex << cur_cluster->get_cluster_id();
					output << " == " << "cluster " << hex << base_cluster->get_cluster_id() << endl;
					output << "Cluster " << hex << cur_cluster->get_cluster_id() << " has #nodes " << hex << cur_cluster->get_norm_nodes().size();
					output  << " groups " << hex << cur_cluster->original_size()<< endl;
					output << "Cluster " << hex << base_cluster->get_cluster_id() << " has #nodes " << hex << base_cluster->get_norm_nodes().size();
					output  << " groups " << hex << base_cluster->original_size() << endl;
				}
				equal++;
				found = true;
				break;
			}
		}

		if (!found) {
			outstanders.push_back(cur_cluster);
		}
	}

	output.close();

	cout << "***** compare summary *****" << endl;
	cout << "total clusters of target = " << std::dec << norm_clusters.size() << endl;
	cout << "compare to base clusters = " << std::dec << base_clusters.size() << endl;
	cout << "equal_groups = " << std::dec << equal << endl;
	cout << "outstanding groups = " << std::dec << outstanders.size() << endl;
	cout << "***************************" << endl;
}

void Normalizer::decode_outstand_cluster(const char * outputfile)
{
	ofstream output(outputfile);
	if (output.fail()) {
		cerr << "Error : fail to open file " << outputfile << endl;
		return;
	}

	vector<norm_cluster_t *>::iterator it;
	
	output << "Total number of Clusters " << std::dec << outstanders.size() << endl;
	for (it = outstanders.begin(); it != outstanders.end(); it++) {
		output << "#Cluster " << hex << (*it)->get_cluster_id() << endl;
		(*it)->decode_cluster(output);
	}
	output.close();
}

vector<norm_group_t *> Normalizer::get_groups(void)
{
	vector<norm_group_t *> ret;
	ret.clear();
	map<uint64_t, norm_cluster_t *>::iterator it;
	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++) {
		norm_cluster_t * cur_cluster = it->second;
		ret.insert(ret.end(), cur_cluster->get_norm_nodes().begin(), cur_cluster->get_norm_nodes().end());
	}
	return ret;
}

void Normalizer::check_group(norm_group_t * group, vector<norm_group_t *> & base_groups)
{
	vector<norm_group_t *>::iterator it;
	for (it = base_groups.begin(); it != base_groups.end(); it++) {
		if (*group == **it) {
			group->set_norm();
			break;
		}
	}
}

void Normalizer::compare_groups(Normalizer * base)
{
	vector<norm_group_t *> this_groups = get_groups();
	vector<norm_group_t *> base_groups = base->get_groups();
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &ioService));

	vector<norm_group_t *>::iterator it;
	for (it = this_groups.begin(); it != this_groups.end(); it++)
		ioService.post(boost::bind(&Normalizer::check_group, this, *it, base_groups));

	work.reset();
	threadpool.join_all();
	//ioService.stop();
}
