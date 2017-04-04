#include "parser.hpp"
#include "mach_msg.hpp"
#include "mkrun.hpp"
#include "interrupt.hpp"
#include "workq_next.hpp"
#include "tsmaintenance.hpp"
#include "syscall.hpp"
#include "dispatch.hpp"
#include "timer_callout.hpp"
#include "voucher.hpp"
#include "backtraceinfo.hpp"
#include "eventlistop.hpp"
#include "group.hpp"
#include "cluster.hpp"
#include "cluster_filter.hpp"
#include "thread_divider.hpp"
//#include "timercall_divider.hpp"
#include <time.h>

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

int	main(int argc, char* argv[]) {
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
						   .libinfo_file = "./input/libinfo.log", /*optional*/
						   .procs_file = "./input/current_procs.log", /*optional*/
						   .intersectfile = "./input/process_intersection.log", /*optional*/
						   .host = appname,
						   .pid = pid,
						   .suspicious_api = "CGSConnectionSetSpinning",
						   .nthreads = 2};

	LoadData::preload();
	
	/* output files */
	string streamout_file = get_prefix(logfile) + "_allevent.stream";
	string eventdump_file = get_prefix(logfile) + "_allevent.decode";

	string stream_groups = get_prefix(logfile) + "_groups.stream";
	string decode_groups = get_prefix(logfile) + "_groups.decode";

	string stream_clusters = get_prefix(logfile) + "_clusters.stream";
	string decode_clusters = get_prefix(logfile) + "_clusters.decode";
	string stream_filtered_clusters = get_prefix(logfile) + "_filtered_clusters.stream";

	/*
	string streamout_mthread_group = get_prefix(logfile) + "_mthread_groups.stream";
	string decode_mthread_group = get_prefix(logfile) + "_mthread_groups.decode";
	string streamout_nsethread_group = get_prefix(logfile) + "_nsethread_groups.stream";
	string decode_nsethread_group = get_prefix(logfile) + "_nsethread_groups.decode";
	*/
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
	cout << "begin grouping and filling connectors ..." << endl;
	time(&time_begin);
	groups_t * g_ptr = new groups_t(event_lists);
	g_ptr->para_group();

	cout << "Decode groups ... " << endl;
	g_ptr->decode_groups(decode_groups);
	g_ptr->streamout_groups(stream_groups);

	time(&time_end);
	cout << "Time cost for grouping " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/*
	MainThreadDivider mthreaddivider(g_ptr->get_list_of_uimain());
	mthreaddivider.divide();
	mthreaddivider.streamout_groups(streamout_mthread_group);
	mthreaddivider.decode_groups(decode_mthread_group);
	cout << "Finished dividing UI Main thread ... " << endl;
		
	NSEventThreadDivider nsethreaddivider(g_ptr->get_list_of_nsevent());
	nsethreaddivider.divide();
	nsethreaddivider.streamout_groups(streamout_nsethread_group);
	nsethreaddivider.decode_groups(decode_nsethread_group);
	cout << "Finished dividing UI NSevent thread ..." << endl;
	*/
	

	/* clustering
	cout << "begin merge groups into clusters..." << endl;
	time(&time_begin);
	clusters_t *c_ptr = new clusters_t(g_ptr);
	c_ptr->merge_by_mach_msg();
	c_ptr->merge_by_dispatch_ops();
	c_ptr->merge_by_mkrun();
	c_ptr->merge_by_timercallout();
	cout << "Decode clusters ... " << endl;
	c_ptr->streamout_clusters(stream_clusters);
	time(&time_end);
	cout << "Time cost for clustering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	*/

	cout << "begin merge groups into clusters..." << endl;
	time(&time_begin);
	ClusterGen *c_ptr = new ClusterGen(g_ptr);
	cout << "Decode clusters ... " << endl;
	c_ptr->streamout_clusters(stream_clusters);
	time(&time_end);
	cout << "Time cost for clustering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	

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
	EventListOp::dump_all_event(event_lists[0], eventdump_file.c_str());
	time(&time_end);
	cout << "Time cost for decodeing original cluster" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	

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
