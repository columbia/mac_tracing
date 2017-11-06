#include "parser.hpp"
#include "backtraceinfo.hpp"
#include "group.hpp"
#include <time.h>

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

int	main(int argc, char* argv[]) {
	if (argc < 5) {
		cerr << "[usage]: " << argv[0] << "log_file_recorded libs_dir app pid" << endl;
		exit(EXIT_FAILURE);
	}
	string logfile = argv[1];
	string libsdir = argv[2];
	string appname = argv[3];
	pid_t pid = atoi(argv[4]);

	cout << "Checking spinning application from " << logfile << endl;

	/* load */
	LoadData::meta_data = {.datafile = logfile,
						   .libs_dir = libsdir,
						   .libinfo_file = "./input/libinfo.log", /*optional*/
						   .procs_file = "./input/current_procs.log", /*optional*/
						   .intersectfile = "./input/process_intersection.log", /*optional*/
						   .tpc_maps_file = "./input/tpcmap.log",
						   .host = appname,
						   .pid = pid,
						   .suspicious_api = "CGSConnectionSetSpinning",
						   .nthreads = 6};

	/*generate libinfo_file*/
	if (LoadData::meta_data.libs_dir.size() || access(LoadData::meta_data.libs_dir.c_str(), R_OK)) {
		cerr << "No lib info provided " << endl;
		return -1;
	}

	LoadData::load_all_libs();
	LoadData::preload();
	
	time_t time_begin, time_end;
	/* parser */
	cout << "Begin parse..." << endl;
	lldb::SBDebugger::Initialize();
	time(&time_begin);
	list<event_t *> event_list = Parse::parse_backtrace();
	//map<uint64_t, list<event_t *>> event_lists = Parse::divide_and_parse();
	lldb::SBDebugger::Terminate();
	time(&time_end);
	cout << "Time cost for parsing " << fixed << setprecision(2) << difftime(time_end, time_begin) << "seconds"<< endl;

	cout << "Clearing events..." << endl;
	EventListOp::clear_event_list(event_list);
	cout << "Done!" << endl;
	return 0;
}
