#include "norm_cluster.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define DEBUG_NORM 1
typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Normalizer::Normalizer(map<uint64_t, cluster_t *> &_clusters)
:clusters(_clusters)
{
	key_events.insert(make_pair(MACH_IPC_MSG, true));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_INFO, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_CONN, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_TRANSIT, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_DEALLOC, false));
	key_events.insert(make_pair(MACH_BANK_ACCOUNT, false));
	key_events.insert(make_pair(MACH_MK_RUN, false));
	key_events.insert(make_pair(INTR, false));
	key_events.insert(make_pair(WQ_NEXT, false));
	key_events.insert(make_pair(MACH_TS, false));
	key_events.insert(make_pair(MACH_WAIT, false));
	key_events.insert(make_pair(DISP_ENQ, true));
	key_events.insert(make_pair(DISP_DEQ, true));
	key_events.insert(make_pair(DISP_EXE, true));
	key_events.insert(make_pair(MACH_CALLCREATE, true));
	key_events.insert(make_pair(MACH_CALLOUT, true));
	key_events.insert(make_pair(MACH_CALLCANCEL, true));
	key_events.insert(make_pair(BACKTRACE, false));
	key_events.insert(make_pair(MACH_SYS, true));
	key_events.insert(make_pair(BSD_SYS, true));
	norm_clusters.clear();
}

Normalizer::~Normalizer(void)
{
	map<uint64_t, norm_cluster_t *>::iterator it;

	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++) {
		assert(it->second != NULL);
		delete(it->second);
	}
	norm_clusters.clear();
	key_events.clear();
}

void Normalizer::normalize_cluster(uint64_t index, cluster_t *cluster)
{
	norm_cluster_t *new_cluster = new NormCluster(cluster, key_events);

	if (!new_cluster) {
		cerr << "OOM, unable to allocate memory for normalize cluster" << endl;
		exit(EXIT_FAILURE);
	}

	if ((new_cluster->get_sort_vertexes()).size())
		norm_clusters[index] = new_cluster;
	else
		delete new_cluster;
}

void Normalizer::normalize_clusters(void)
{
#if 0
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	cerr << "Total number of clusters " << hex << clusters.size() << endl;

	map<uint64_t, cluster_t *>::iterator it;
	for (it = clusters.begin(); it != clusters.end(); it++)
		norm_clusters[it->first] = (norm_cluster_t *)NULL;

	for (it = clusters.begin(); it != clusters.end(); it++)
		ioService.post(boost::bind(&Normalizer::normalize_cluster, this, it->first, it->second));

	work.reset();
	threadpool.join_all();
#endif
	map<uint64_t, cluster_t *>::iterator it;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		normalize_cluster(it->first, it->second);
	}

#if DEBUG_NORM
	map<uint64_t, norm_cluster_t *>::iterator norm_it;
	for (norm_it = norm_clusters.begin(); norm_it != norm_clusters.end();) {
		if (norm_it->second == NULL)
			norm_it = norm_clusters.erase(norm_it);
		else
			norm_it++;
	}
	mtx.lock();
	cerr << "Normalized " << hex << norm_clusters.size() << " clusters" << endl;
	mtx.unlock();
#endif
}

void Normalizer::sort_clusters(void)
{
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread( boost::bind(&boost::asio::io_service::run, &ioService));

	map<uint64_t, norm_cluster_t *>::iterator it;
	for (it = norm_clusters.begin(); it != norm_clusters.end(); it++)
		ioService.post(boost::bind(&NormCluster::topological_sort, it->second));

	work.reset();
	threadpool.join_all();
}

void Normalizer::compare(Normalizer *base, const char *output_file)
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
		norm_cluster_t *cur_cluster = cur_it->second;
		for (base_it = base_clusters.begin(); base_it != base_clusters.end(); base_it++) {
			norm_cluster_t *base_cluster = base_it->second;
			
			if (*cur_cluster == *base_cluster) {
				output << "cluster " << hex << cur_cluster->get_cluster_id();
				output << " == " << "cluster " << hex << base_cluster->get_cluster_id() << endl;
				output << "Cluster " << hex << cur_cluster->get_cluster_id() << " has #nodes " << hex << cur_cluster->get_sort_vertexes().size();
				output  << " groups " << hex << cur_cluster->original_size()<< endl;
				output << "Cluster " << hex << base_cluster->get_cluster_id() << " has #nodes " << hex << base_cluster->get_sort_vertexes().size();
				output  << " groups " << hex << base_cluster->original_size() << endl;
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
	cout << "equal clusters = " << std::dec << equal << endl;
	cout << "outstanding clusters = " << std::dec << outstanders.size() << endl;
	cout << "***************************" << endl;
}

void Normalizer::decode_outstand_cluster(const char *outputfile)
{
	ofstream output(outputfile);
	if (output.fail()) {
		cerr << "Error: fail to open file " << outputfile << endl;
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
