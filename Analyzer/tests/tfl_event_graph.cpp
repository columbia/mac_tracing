#include "parser.hpp"
#include "group.hpp"
#include "graph.hpp"
#include "canonization.hpp"
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <unistd.h>
#include <boost/filesystem.hpp>

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

void handler(int sig)
{
    void *btarray[16];
    size_t size = backtrace(btarray, 16);
    mtx.lock();
    std::cerr << "Signal " << sig << std::endl;
    backtrace_symbols_fd(btarray, size, STDERR_FILENO);
    mtx.unlock();
    exit(1);
}

static time_t time_begin, time_end;

int main(int argc, char* argv[]) {
    signal(SIGSEGV, handler);
    signal(SIGABRT, handler);
    signal(SIGINT, handler);
    if (argc < 4) {
        std::cerr << "[usage]: " << argv[0] << " log_file  name_of_app  pid_of_app(0 for none local symbolication)" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string logfile = argv[1];
    std::string appname = argv[2];
    pid_t pid = atoi(argv[3]);
    std::cout << "Analyzing " << appname << " [" << std::dec << pid << "] spinning for " << logfile << std::endl;

    LoadData::meta_data.data = logfile;
    LoadData::meta_data.host = appname;
    LoadData::meta_data.pid = pid;
    LoadData::meta_data.nthreads = 6;

/*
    EventLists *event_lists_ptr = new EventLists(LoadData::meta_data);
    Groups *g_ptr = new Groups(event_lists_ptr);
    std::string EventBasefl_path = get_prefix(logfile) + "_tfl.log";
    event_lists_ptr->tfl_all_event(EventBasefl_path);

    std::cout << "Clearing groups..." << std::endl;
    delete g_ptr;

    std::cout << "Clearing events..." << std::endl;
    delete event_lists_ptr;
*/
    TransactionGraph *graph = new TransactionGraph(get_prefix(logfile));
    std::string tfl_path = get_prefix(logfile)+"_graph.";
    graph->tfl_nodes_and_edges(tfl_path);
    std::cout << "Clearing graph..." << std::endl;
    delete graph;
    std::cout << "Done!" << std::endl;
    return 0;
}
