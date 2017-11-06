#include "parser.hpp"
#include "group.hpp"
#include "cluster.hpp"
#include "cluster_filter.hpp"
#include "search_engine.hpp"
#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <unistd.h>

mutex mtx;

static string get_prefix(string &input_path)
{
	string filename;
	size_t dir_pos = input_path.find_last_of("/");
	if (dir_pos != string::npos) {
		filename = input_path.substr(dir_pos + 1);
	} else {
		filename = input_path;
	}

	size_t pos = filename.find(".");
	if (pos != string::npos)
		return filename.substr(0, pos);
	else
		return filename;
}

void handler(int sig)
{
	void *btarray[10];
	size_t size = backtrace(btarray, 10);
	mtx.lock();
	backtrace_symbols_fd(btarray, size, STDERR_FILENO);
	mtx.unlock();
	exit(1);
}

int	main(int argc, char* argv[]) {
	signal(SIGSEGV, handler);
	signal(SIGABRT, handler);
	if (argc < 4) {
		cerr << "[usage]: " << argv[0] << "log_file_recorded  APP_name  pid_of_app_current_running" << endl;
		exit(EXIT_FAILURE);
	}
	string logfile = argv[1];
	string appname	= argv[2];
	pid_t pid = atoi(argv[3]);
	cout << "Analyzing " << appname << "[" << dec << pid << "] spinning for " << logfile << endl;

	/* load */
	LoadData::meta_data = {.datafile = logfile,
						   .libs_dir = "./input/libs", /*optional*/
						   .libinfo_file = "./input/libinfo.log", /*optional*/
						   .procs_file = "./input/current_procs.log", /*optional*/
						   .intersectfile = "./input/process_intersection.log", /*optional*/
						   .tpc_maps_file = "./input/tpcmap.log",
						   .host = appname,
						   .pid = pid,
						   .suspicious_api = "CGSConnectionSetSpinning",
						   .nthreads = 6};

	/*regnerate lib info */
	cerr << "Regnerate lib info" << endl;
	if (LoadData::meta_data.libs_dir.size() && access(LoadData::meta_data.libs_dir.c_str(), R_OK) == 0) {
		if (remove(LoadData::meta_data.libinfo_file.c_str()))
			perror("Error in deleting file");
		if (!LoadData::load_lib(LoadData::meta_data.host))
			cerr << "Fail to load lib for " << LoadData::meta_data.host << endl;
		if (!LoadData::load_lib("WindowServer"))
			cerr << "Fail to load lib for WindowServer" << endl;
	}
	cerr << "Regnerated lib info" << endl;
	
	LoadData::preload();
	/* output files */
	string streamout_file = get_prefix(logfile) + "_allevent.stream";
	string eventdump_file = get_prefix(logfile) + "_allevent.decode";

	string stream_groups = get_prefix(logfile) + "_groups.stream";
	string decode_groups = get_prefix(logfile) + "_groups.decode";

	string stream_clusters = get_prefix(logfile) + "_clusters.stream";
	string inspect_clusters = get_prefix(logfile) + "_cluster.inspect";
	string pic_cluster = get_prefix(logfile) + "_cluster_for_pic.js";
	string decode_clusters = get_prefix(logfile) + "_clusters.decode";
	string stream_filtered_clusters = get_prefix(logfile) + "_filtered_clusters.stream";

	time_t time_begin, time_end;

	/* parser */
	cout << "Begin parse..." << endl;
	lldb::SBDebugger::Initialize();
	time(&time_begin);
	map<uint64_t, list<event_t *>> event_lists = Parse::divide_and_parse();
	lldb::SBDebugger::Terminate();
	time(&time_end);
	cout << "Time cost for parsing " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;


	/* grouping */
	time(&time_begin);
	cout << "Begin grouping ... " << endl;
	groups_t * g_ptr = new groups_t(event_lists);
	cout << "Decode groups ... " << endl;
	//g_ptr->decode_groups(decode_groups);
	g_ptr->streamout_groups(stream_groups);
	time(&time_end);
	cout << "Time cost for grouping " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds" << endl;

	/*clustering*/
	cout << "begin merge groups into clusters..." << endl;
	time(&time_begin);
	ClusterGen *c_ptr = new ClusterGen(g_ptr);
	cout << "Decode clusters ... " << endl;
	c_ptr->streamout_clusters(stream_clusters);
	//cout << "Inspect clusters... " << endl;
	//c_ptr->inspect_clusters(inspect_clusters);
	//cout << "Pic clusters..." << endl;
	//c_ptr->pic_clusters(pic_cluster);
	time(&time_end);
	cout << "Time cost for clustering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	BugSearcher search_engine(c_ptr);
	cluster_t *cluster = search_engine.report_spinning_cluster();
	group_t *suspicious_group = NULL;
	event_t *suspicious_block_event = NULL;
	
	if (cluster) {
		cerr << "Get suspicious cluster" << endl;
		suspicious_group = search_engine.suspicious_segment(cluster);
		suspicious_block_event = search_engine.suspicious_blocking(cluster);
		if (suspicious_group)
			cout << "Suspicious group " << hex << suspicious_group->get_group_id() << endl;
		if (suspicious_block_event)
			cout << "Suspicious wait " << fixed << setprecision(1) << suspicious_block_event->get_abstime() << endl;
	}

	if (!suspicious_group && !suspicious_block_event)
		cout << "Fail to diagnose" << endl;

#if FILTER
	cout << "Time cost for decodeing original cluster" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	cout << "begin filter cluster" << endl;
	Filter * filter_ptr = new Filter(c_ptr);
	time(&time_begin);
	filter_ptr->clusters_filter_para();
	time(&time_end);
	cout << "Time cost for filtering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	time(&time_begin);
	cout << "Decode filted cluster ... " << endl;
	filter_ptr->streamout_filtered_clusters(stream_filtered_clusters);
	time(&time_end);
	cout << "Time cost for decodeing filted cluster" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
#endif

	/* decoding */
	time(&time_begin);
	cout << "Decode event list... " << endl;
	EventListOp::streamout_all_event(event_lists[0], streamout_file.c_str());
	//EventListOp::dump_all_event(event_lists[0], eventdump_file.c_str());
	time(&time_end);
	cout << "Time cost for decodeing event list" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	

	/* clearing */
	#if FILTER
	cout << "Clearing filter ... " << endl;
	delete(filter_ptr);
	#endif

	cout << "Clearing clusters ..." << endl;
	delete(c_ptr);
	cout << "Clearing groups..." << endl;
	delete(g_ptr);
	cout << "Clearing events..." << endl;
	EventListOp::clear_event_list(event_lists[0]);
	cout << "Done!" << endl;
	return 0;
}
