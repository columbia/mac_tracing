#include "parser.hpp"
#include "group.hpp"
#include "canonization.hpp"
//#include "cluster.hpp"
#include "search_engine.hpp"
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <unistd.h>
#include <boost/filesystem.hpp>

mutex mtx;

static string get_prefix(string &input_path)
{
	boost::filesystem::path outdir("./output/");
	if (!(boost::filesystem::exists(outdir)))  
		boost::filesystem::create_directory(outdir);

	string filename;
	size_t dir_pos = input_path.find_last_of("/");
	if (dir_pos != string::npos) {
		filename = input_path.substr(dir_pos + 1);
	} else {
		filename = input_path;
	}

	size_t pos = filename.find(".");
	if (pos != string::npos)
		return outdir.c_str() + filename.substr(0, pos);
	else
		return outdir.c_str() + filename;
}

void handler(int sig)
{
	void *btarray[16];
	size_t size = backtrace(btarray, 16);
	mtx.lock();
	backtrace_symbols_fd(btarray, size, STDERR_FILENO);
	mtx.unlock();
	exit(1);
}

int main(int argc, char* argv[]) {
	signal(SIGSEGV, handler);
	signal(SIGABRT, handler);
	if (argc < 4) {
		cerr << "[usage]: " << argv[0] << "log_file_recorded  APP_name  pid_of_app_current_running" << endl;
		exit(EXIT_FAILURE);
	}
	string logfile = argv[1];
	string appname = argv[2];
	pid_t pid = atoi(argv[3]);
	cout << "Analyzing " << appname << " [" << dec << pid << "] spinning for " << logfile << endl;

	LoadData::meta_data.data = logfile;
	LoadData::meta_data.host = appname;
	LoadData::meta_data.pid = pid;
	LoadData::meta_data.nthreads = 6;
	EventLists *event_lists_ptr = new EventLists(LoadData::meta_data);

	cout << "Begin grouping ..." << endl;
	groups_t *g_ptr = new groups_t(event_lists_ptr);

	cout << "Decode groups ..." << endl;
	string group_stream_path = get_prefix(logfile) + "_groups.stream";
	g_ptr->streamout_groups(group_stream_path);

	cout << "Decode event list..." << endl;
	string event_stream_path = get_prefix(logfile) + "_events.stream";
	event_lists_ptr->streamout_all_event(event_stream_path);
	
	cout << "Search bug..." << endl;
	string report_path = get_prefix(logfile) + "_blocks.stream";
	string buggy_path = get_prefix(logfile) + "_paths.stream";
	BugSearcher bug_searcher(g_ptr);
	map<wait_ev_t *, double> result = bug_searcher.suspicious_blocking(g_ptr, report_path);
	bug_searcher.slice_path(-1, buggy_path);

	cout << "Clearing groups..." << endl;
	delete(g_ptr);

	cout << "Clearing events..." << endl;
	delete(event_lists_ptr);

	cout << "Done!" << endl;
	return 0;
}
