#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <unistd.h>
#include <boost/filesystem.hpp>

#include "parser.hpp"
#include "group.hpp"
#include "canonization.hpp"
#include "graph.hpp"
#include "search_graph.hpp"
#include "critical_path.hpp"


std::mutex mtx;

static std::string get_prefix(std::string &input_path)
{
    boost::filesystem::path outdir("./output/");
    if (!(boost::filesystem::exists(outdir)))  
        boost::filesystem::create_directory(outdir);

    std::string filename;
    size_t dir_pos = input_path.find_last_of("/");
    if (dir_pos != std::string::npos) {
        filename = input_path.substr(dir_pos + 1);
    } else {
        filename = input_path;
    }

    size_t pos = filename.find(".");
    if (pos != std::string::npos)
        return outdir.c_str() + filename.substr(0, pos);
    else
        return outdir.c_str() + filename;
}


static void debug_handler(int sig)
{
    void *btarray[16];
    size_t size = backtrace(btarray, 16);
    std::cerr << "Signal " << sig << std::endl;
    backtrace_symbols_fd(btarray, size, STDERR_FILENO);
    exit(1);
}


int main(int argc, char* argv[]) {
    signal(SIGSEGV, debug_handler);
    signal(SIGABRT, debug_handler);
    signal(SIGINT, debug_handler);

    if (argc < 4) {
        std::cerr << "[usage]: " << argv[0];
        std::cerr << " log_file  name_of_app  pid_of_app(0 for none local symbolication)" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string logfile = argv[1];
    std::string appname = argv[2];
    pid_t pid = atoi(argv[3]);

    LoadData::meta_data.data = logfile;
    LoadData::meta_data.host = appname;
    LoadData::meta_data.pid = pid;
    LoadData::meta_data.nthreads = 6;

    std::cout << "Analyzing " << appname << " [" << std::dec << pid << "] on " << logfile << std::endl;

    EventGraph *graph = new EventGraph(get_prefix(logfile));
#if 0
    //TransactionGraph *graph = new TransactionGraph(get_prefix(logfile));
    //uint64_t end_gid = graph->get_end_node()->get_gid();
    //std::cout << "Transaction graph generated to 0x";
    //std::cout << std::hex << end_gid << std::endl;
    CriticalPath *paths = new CriticalPath(graph);
    paths->extract_path();
    delete paths;
#endif

    
    std::cout << "Construct search engine..." << std::endl;
    std::string js_path = get_prefix(logfile) + "_graph.js";
    std::map<EventType::event_type_t, bool> key_events;
    bool interference = true;
    key_events.clear();
    GraphSearcher *search_engine = new GraphSearcher(graph, key_events);
    search_engine->examin_spinning_cursor(js_path, interference);
    std::cout << "Clearing toolkit..." << std::endl;
    delete search_engine;
    std::string graph_path = get_prefix(logfile) + "_graph.streamout";
    graph->streamout_nodes_and_edges(graph_path);
    std::cout << "Clearing graph..." << std::endl;
    delete graph;
    return 0;
}
