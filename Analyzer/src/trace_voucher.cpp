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
#include "timercall_divider.hpp"
#include "voucher_connection.hpp"
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
	string eventdump_voucher_file = get_prefix(logfile) + "_voucher.decode";
	string dump_clusters = get_prefix(logfile) + "_clusters.decode";
	string stream_clusters = get_prefix(logfile) + "_clusters.stream";
	string stream_dispatch_events = get_prefix(logfile) + "_dispatchs.stream";
	string stream_filtered_clusters = get_prefix(logfile) + "_filtered_clusters.stream";
	string decode_groups = get_prefix(logfile) + "_groups.decode";
	string stream_groups = get_prefix(logfile) + "_groups.stream";
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
	cout << "Begin grouping .. " << endl;
	time(&time_begin);
	groups_t * g_ptr = new groups_t(event_lists);
	g_ptr->para_group();
	time(&time_end);
	cout << "Total group size : " << (g_ptr->get_groups()).size() << endl;
	cout << "Time cost for grouping " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/* clustering */
	cout << "begien clustering...\nbegin fill connector " << endl;
	time(&time_begin);
	clusters_t *c_ptr = new clusters_t(g_ptr);
	cout << "begin merge " << endl;
	c_ptr->merge_by_mach_msg();
	c_ptr->merge_by_dispatch_ops();
	c_ptr->merge_by_mkrun();
	c_ptr->merge_by_timercallout();
	time(&time_end);
	cout << "Time cost for clustering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/*voucher_connection*/
	list<event_t *> voucher_list;
	voucher_list.insert(voucher_list.end(), event_lists[MACH_IPC_MSG].begin(), event_lists[MACH_IPC_MSG].end());
	voucher_list.insert(voucher_list.end(), event_lists[MACH_IPC_VOUCHER_INFO].begin(), event_lists[MACH_IPC_VOUCHER_INFO].end());
	voucher_list.insert(voucher_list.end(), event_lists[MACH_IPC_VOUCHER_CONN].begin(), event_lists[MACH_IPC_VOUCHER_CONN].end());
	voucher_list.insert(voucher_list.end(), event_lists[MACH_IPC_VOUCHER_TRANSIT].begin(), event_lists[MACH_IPC_VOUCHER_TRANSIT].end());
	voucher_list.insert(voucher_list.end(), event_lists[MACH_IPC_VOUCHER_DEALLOC].begin(), event_lists[MACH_IPC_VOUCHER_DEALLOC].end());
	EventListOp::sort_event_list(voucher_list);
	IPCForest * forest = new IPCForest(voucher_list);
	forest->construct_forest();
	forest->decode_voucher_relations((get_prefix(logfile) +"_voucher_relationships.log").c_str());
	forest->check_remain_incomplete_nodes();
	forest->clear();//memory leak because no implementstion
	delete (forest);

	/*decoding*/
	#if FILTER_CLUSTER
	cout << "Decode original cluster ... " << endl;
	c_ptr->streamout_clusters(stream_clusters);

	cout << "begin filter cluster" << endl;
	Filter * filter_ptr = new Filter(c_ptr);
	time(&time_begin);
	filter_ptr->clusters_filter_para();
	time(&time_end);
	cout << "Time cost for filtering " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	cout << "Decode filted cluster ... " << endl;
	filter_ptr->streamout_filtered_clusters(stream_filtered_clusters);
	#endif


	cout << "Decode lists ... " << endl;
	EventListOp::streamout_all_event(event_lists[0], streamout_file.c_str());
	EventListOp::dump_all_event(event_lists[0], eventdump_file.c_str());
	EventListOp::dump_all_event(event_lists[MACH_IPC_VOUCHER_INFO], eventdump_voucher_file.c_str());

	cout << "Decode groups ... " << endl;
	g_ptr->decode_groups(decode_groups);
	g_ptr->streamout_groups(stream_groups);

	#if CHECK_DISPATH
	list<event_t *> dispath_list;
	dispath_list.insert(dispath_list.end(), event_lists[DISP_ENQ].begin(), event_lists[DISP_ENQ].end());
	dispath_list.insert(dispath_list.end(), event_lists[DISP_DEQ].begin(), event_lists[DISP_DEQ].end());
	dispath_list.insert(dispath_list.end(), event_lists[DISP_EXE].begin(), event_lists[DISP_EXE].end());
	EventListOp::sort_event_list(dispath_list);
	EventListOp::streamout_all_event(dispath_list, stream_dispatch_events.c_str());
	#endif

	time(&time_end);
	cout << "Time cost for decodeing " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	/* Alternative diffing inside one trace of repeated actions */
	#if DIVIDE
	cerr << "Try divide..." << endl;
	TimerCallDivider divider(event_lists[0], event_lists[BACKTRACE], 0, NULL);
	divider.divide();
	divider.compare();
	cerr << "End of divide and compare..." << endl;
	#endif

	/* clearing */
	#if FILTER_CLUSTER
	cout << "Clearing filter ... " << endl;
	delete(filter_ptr);
	#endif 

	cout << "Clearing clusters ... " << endl;
	c_ptr->clear_clusters();
	delete(c_ptr);
	cout << "Clearing groups..." << endl;
	g_ptr->clear_groups();
	delete(g_ptr);
	cout << "Clearing events..." << endl;
	EventListOp::clear_event_list(event_lists[0]);
	cout << "Done!" << endl;
	return 0;
}
