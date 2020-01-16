#include "NormGroupcluster.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define DEBUG_NORM 1
typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

Normalizer::Normalizer(std::map<uint64_t, cluster_t *> &_clusters)
:clusters(_clusters)
{
    key_events.insert(std::make_pair(MACH_IPC_MSG, true));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_INFO, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_CONN, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_TRANSIT, false));
    key_events.insert(std::make_pair(MACH_IPC_VOUCHER_DEALLOC, false));
    key_events.insert(std::make_pair(MACH_BANK_ACCOUNT, false));
    key_events.insert(std::make_pair(MACH_MK_RUN, false));
    key_events.insert(std::make_pair(INTR, false));
    key_events.insert(std::make_pair(WQ_NEXT, false));
    key_events.insert(std::make_pair(MACH_TS, false));
    key_events.insert(std::make_pair(MACH_WAIT, false));
    key_events.insert(std::make_pair(DISP_ENQ, true));
    key_events.insert(std::make_pair(DISP_DEQ, true));
    key_events.insert(std::make_pair(DISP_EXE, true));
    key_events.insert(std::make_pair(MACH_CALLCREATE, true));
    key_events.insert(std::make_pair(MACH_CALLOUT, true));
    key_events.insert(std::make_pair(MACH_CALLCANCEL, true));
    key_events.insert(std::make_pair(BACKTRACE, false));
    key_events.insert(std::make_pair(MACH_SYS, true));
    key_events.insert(std::make_pair(BSD_SYS, true));
    NormGroupclusters.clear();
}

Normalizer::~Normalizer(void)
{
    std::map<uint64_t, NormGroupcluster_t *>::iterator it;

    for (it = NormGroupclusters.begin(); it != NormGroupclusters.end(); it++) {
        assert(it->second != nullptr);
        delete(it->second);
    }
    NormGroupclusters.clear();
    key_events.clear();
}

void Normalizer::normalize_cluster(uint64_t index, cluster_t *cluster)
{
    NormGroupcluster_t *new_cluster = new NormCluster(cluster, key_events);

    if (!new_cluster) {
        std::cerr << "OOM, unable to allocate memory for normalize cluster" << std::endl;
        exit(EXIT_FAILURE);
    }

    if ((new_cluster->get_sort_vertexes()).size())
        NormGroupclusters[index] = new_cluster;
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

    std::cerr << "Total number of clusters " << std::hex << clusters.size() << std::endl;

    std::map<uint64_t, cluster_t *>::iterator it;
    for (it = clusters.begin(); it != clusters.end(); it++)
        NormGroupclusters[it->first] = (NormGroupcluster_t *)nullptr;

    for (it = clusters.begin(); it != clusters.end(); it++)
        ioService.post(boost::bind(&Normalizer::normalize_cluster, this, it->first, it->second));

    work.reset();
    threadpool.join_all();
#endif
    std::map<uint64_t, cluster_t *>::iterator it;
    for (it = clusters.begin(); it != clusters.end(); it++) {
        normalize_cluster(it->first, it->second);
    }

#if DEBUG_NORM
    std::map<uint64_t, NormGroupcluster_t *>::iterator NormGroupit;
    for (NormGroupit = NormGroupclusters.begin(); NormGroupit != NormGroupclusters.end();) {
        if (NormGroupit->second == nullptr)
            NormGroupit = NormGroupclusters.erase(NormGroupit);
        else
            NormGroupit++;
    }
    mtx.lock();
    std::cerr << "Normalized " << std::hex << NormGroupclusters.size() << " clusters" << std::endl;
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

    std::map<uint64_t, NormGroupcluster_t *>::iterator it;
    for (it = NormGroupclusters.begin(); it != NormGroupclusters.end(); it++)
        ioService.post(boost::bind(&NormCluster::topological_sort, it->second));

    work.reset();
    threadpool.join_all();
}

void Normalizer::compare(Normalizer *base, const char *output_file)
{
    std::ofstream output(output_file);
    if (output.fail()) {
        std::cerr << "Error : fail to open file " << output_file << std::endl;
        return;
    }

    std::map<uint64_t, NormGroupcluster_t *> &base_clusters = base->get_normed_clusters();
    std::map<uint64_t, NormGroupcluster_t *>::iterator base_it;
    std::map<uint64_t, NormGroupcluster_t *>::iterator cur_it;
    uint32_t equal = 0;

    for (cur_it = NormGroupclusters.begin(); cur_it != NormGroupclusters.end(); cur_it++) {
        bool found =  false;
        NormGroupcluster_t *cur_cluster = cur_it->second;
        for (base_it = base_clusters.begin(); base_it != base_clusters.end(); base_it++) {
            NormGroupcluster_t *base_cluster = base_it->second;
            
            if (*cur_cluster == *base_cluster) {
                output << "cluster " << std::hex << cur_cluster->get_cluster_id();
                output << " == " << "cluster " << std::hex << base_cluster->get_cluster_id() << std::endl;
                output << "Cluster " << std::hex << cur_cluster->get_cluster_id() << " has #nodes " << std::hex << cur_cluster->get_sort_vertexes().size();
                output  << " groups " << std::hex << cur_cluster->original_size()<< std::endl;
                output << "Cluster " << std::hex << base_cluster->get_cluster_id() << " has #nodes " << std::hex << base_cluster->get_sort_vertexes().size();
                output  << " groups " << std::hex << base_cluster->original_size() << std::endl;
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

    std::cout << "***** compare summary *****" << std::endl;
    std::cout << "total clusters of target = " << std::dec << NormGroupclusters.size() << std::endl;
    std::cout << "compare to base clusters = " << std::dec << base_clusters.size() << std::endl;
    std::cout << "equal clusters = " << std::dec << equal << std::endl;
    std::cout << "outstanding clusters = " << std::dec << outstanders.size() << std::endl;
    std::cout << "***************************" << std::endl;
}

void Normalizer::decode_outstand_cluster(const char *outputfile)
{
    std::ofstream output(outputfile);
    if (output.fail()) {
        std::cerr << "Error: fail to open file " << outputfile << std::endl;
        return;
    }

    std::vector<NormGroupcluster_t *>::iterator it;
    
    output << "Total number of Clusters " << std::dec << outstanders.size() << std::endl;
    for (it = outstanders.begin(); it != outstanders.end(); it++) {
        output << "#Cluster " << std::hex << (*it)->get_cluster_id() << std::endl;
        (*it)->decode_cluster(output);
    }
    output.close();
}
