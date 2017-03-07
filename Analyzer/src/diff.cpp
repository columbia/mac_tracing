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
#include "canonization.hpp"
#include "cluster_filter.hpp"
#include "divider.hpp"
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
	if (argc < 5) {
		cerr << "[usage]: " << argv[0] << "log_file_recorded_bug log_file_normal APP_name pid_of_app_current_running" << endl;
		exit(EXIT_FAILURE);
	}

	string logfile = argv[1];
	string lognorm = argv[2];
	string appname	= argv[3];
	pid_t pid = atoi(argv[4]);
	cout << "Analyzing " << appname << "[" << dec << pid << "] spinning for " << logfile << endl;

	/* output files */
	string streamout_file = get_prefix(logfile) + "_allevent.stream";
	string eventdump_file = get_prefix(logfile) + "_allevent.decode";
	string eventdump_voucher_file = get_prefix(logfile) + "_voucher.decode";
	string dump_clusters = get_prefix(logfile) + "_clusters.decode";
	string stream_clusters = get_prefix(logfile) + "_clusters.stream";
	string stream_filtered_clusters = get_prefix(logfile) + "_filtered_clusters.stream";

	string norm_streamout_file = get_prefix(lognorm) + "_allevent.stream";
	string norm_eventdump_file = get_prefix(lognorm) + "_allevent.decode";
	string norm_eventdump_voucher_file = get_prefix(lognorm) + "_voucher.decode";
	string norm_dump_clusters = get_prefix(lognorm) + "_clusters.decode";
	string norm_stream_clusters = get_prefix(lognorm) + "_clusters.stream";
	string norm_stream_filtered_clusters = get_prefix(lognorm) + "_filtered_clusters.stream";

	/* load */
	LoadData::meta_data = {.datafile = logfile,
						   .libinfo_file = "./input/libinfo.log", /*optional*/
						   .procs_file = "./input/current_procs.log", /*optional*/
						   .intersectfile = "./input/process_intersection.log", /*optional*/
						   .host = appname,
						   .pid = pid,
						   .suspicious_api = "CGSConnectionSetSpinning",
						   .nthreads = 4};

	LoadData::preload();
	time_t time_begin, time_end;

	/* parser */
	cout << "Begin parse..." << endl;
	time(&time_begin);
	lldb::SBDebugger::Initialize();

	map<uint64_t, list<event_t *>> event_lists = Parse::divide_and_parse();

	LoadData::meta_data.datafile = lognorm;
	LoadData::meta_data.libinfo_file = "./input/norm_libinfo.log";

	map<uint64_t, list<event_t *>> norm_event_lists = Parse::divide_and_parse();

	lldb::SBDebugger::Terminate();

	time(&time_end);
	cout << "Time cost for parsing " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;


	/* grouping */
	cout << "begin grouping and filling connectors ..." << endl;
	time(&time_begin);
	groups_t * g_ptr = new groups_t(event_lists);
	g_ptr->para_group();
	cout << "Total group size [" << logfile << "]: " << (g_ptr->get_groups()).size() << endl;
	groups_t * norm_g_ptr = new groups_t(norm_event_lists);
	norm_g_ptr->para_group();
	cout << "Total group size [" << lognorm << "]: " << (norm_g_ptr->get_groups()).size() << endl;
	time(&time_end);
	cout << "Time cost for grouping " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/* decoding all event */
	cout << "Decode lists ... " << endl;
	time(&time_begin);
	EventListOp::streamout_all_event(event_lists[0], streamout_file.c_str());
	EventListOp::dump_all_event(event_lists[0], eventdump_file.c_str());
	EventListOp::streamout_all_event(norm_event_lists[0], norm_streamout_file.c_str());
	EventListOp::dump_all_event(norm_event_lists[0], norm_eventdump_file.c_str());
	time(&time_end);
	cout << "Time cost for decodeing " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;


	/* clustering */
	cout << "begin merge groups into clusters..." << endl;
	time(&time_begin);
	clusters_t *c_ptr = new clusters_t(g_ptr);
	c_ptr->merge_by_mach_msg();
	c_ptr->merge_by_dispatch_ops();
	c_ptr->merge_by_mkrun();
	c_ptr->merge_by_callout();
	
	clusters_t * norm_c_ptr = new clusters_t(norm_g_ptr);
	norm_c_ptr->merge_by_mach_msg();
	norm_c_ptr->merge_by_dispatch_ops();
	norm_c_ptr->merge_by_mkrun();
	norm_c_ptr->merge_by_callout();
	time(&time_end);
	cout << "Time cost for clustering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/* decode clusters */
	c_ptr->streamout_clusters(stream_clusters);
	norm_c_ptr->streamout_clusters(norm_stream_clusters);
	

	/* Alternative diffing inside one trace of repeated actions */
//	#if DIVIDE
//	cerr << "Try divide..." << endl;
//	Divider divider(event_lists[0], event_lists[BACKTRACE], 0, NULL);
//	divider.divide();
//	divider.compare();
//	cerr << "End of divide and compare..." << endl;
//	#endif
//
	#define FILTER 1
	#if FILTER
	time(&time_begin);
	cout << "Decode original cluster ... " << endl;
	c_ptr->streamout_clusters(stream_clusters);
	time(&time_end);
	cout << "Time cost for decodeing original cluster" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	cout << "begin filter cluster" << endl;
	Filter * filter_ptr = new Filter(c_ptr);
	Filter * norm_filter_ptr = new Filter(norm_c_ptr);
	time(&time_begin);
	filter_ptr->clusters_filter_para();
	norm_filter_ptr->clusters_filter_para();
	time(&time_end);
	cout << "Time cost for filtering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	time(&time_begin);
	cout << "Decode filted cluster ... " << endl;
	filter_ptr->streamout_filtered_clusters(stream_filtered_clusters);
	time(&time_end);
	cout << "Time cost for decodeing filted cluster" << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;
	#endif

	/* comparing */
	cout << "begin normalize and Compare ..." << endl;
	time(&time_begin);
	cout << "Normalize spin case ..." << endl;
	#if FILTER
	normalizer_t *n_ptr  = new Normalizer(filter_ptr->get_filtered_clusters());
	#else
	normalizer_t *n_ptr  = new Normalizer(c_ptr->get_clusters());
	#endif

	cout << "Normalize normal case .. " << endl;
	#if FILTER
	normalizer_t *norm_n_ptr = new Normalizer(norm_filter_ptr->get_filtered_clusters());
	#else
	normalizer_t *norm_n_ptr = new Normalizer(norm_c_ptr->get_clusters());
	#endif

//	n_ptr->inner_compare((get_prefix(logfile) + "_complexity").c_str());
//	norm_n_ptr->inner_compare((get_prefix(lognorm) + "_complexity").c_str());
	cout << "Comparing ..." << endl;
	n_ptr->compare_groups(norm_n_ptr);
	n_ptr->compare(norm_n_ptr, (get_prefix(logfile) + "_vs_" + get_prefix(lognorm)).c_str());
	n_ptr->decode_outstand_cluster((get_prefix(logfile) + "_subs_" + get_prefix(lognorm)).c_str());
	time(&time_end);

	cout << "Time cost for normalization and comparision " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/* clearing */
	cout << "Clearing normalizer ..." << endl;
	delete(n_ptr);
	delete(norm_n_ptr);
	#if FILTER
	cout << "Clearing filter ... " << endl;
	delete(filter_ptr);
	delete(norm_filter_ptr);
	#endif
	cout << "Clearing clusters ..." << endl;
	delete(c_ptr);
	delete(norm_c_ptr);
	cout << "Clearing groups..." << endl;
	delete(g_ptr);
	delete(norm_g_ptr);
	cout << "Clearing events..." << endl;
	EventListOp::clear_event_list(event_lists[0]);
	cout << "Done!" << endl;
	return 0;
}
