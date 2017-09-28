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
	string check_result = get_prefix(logfile) + "_pattern";

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
	cout << "begin filling connectors ..." << endl;
	time(&time_begin);
	groups_t * g_ptr = new groups_t(event_lists);
	cerr << "begin check pattern ... " << endl;
	g_ptr->check_pattern(check_result);
	time(&time_end);
	cout << "Time cost for checking pattern " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	cout << "Clearing groups..." << endl;
	delete(g_ptr);
	cout << "Clearing events..." << endl;
	EventListOp::clear_event_list(event_lists[0]);
	cout << "Done!" << endl;
	return 0;
}
