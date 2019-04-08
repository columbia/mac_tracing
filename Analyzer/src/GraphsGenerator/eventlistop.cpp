#include "eventlistop.hpp"
#include "mach_msg.hpp"
#include <time.h>
EventLists::EventLists(LoadData::meta_data_t meta_data)
{
	generate_event_lists(meta_data);
}

EventLists::~EventLists()
{
	clear_event_list();
}

map<uint64_t, list<event_t *>> &EventLists::get_event_lists()
{
	return event_lists;
}

int EventLists::generate_event_lists(LoadData::meta_data_t meta_data)
{
	/*regnerate lib info */
	cerr << "Try to remove lib info" << endl;
	if (meta_data.libs_dir.size() && access(meta_data.libs_dir.c_str(), R_OK) == 0) {
		if (!remove(meta_data.libinfo_file.c_str()))
			perror("Fail to delete files");
	}
	cerr << "Regnerate lib info" << endl;
	if (meta_data.libs_dir.size() && access(meta_data.libs_dir.c_str(), R_OK) == 0) {
		ifstream input(meta_data.libs, ifstream::in);
		if (!input.is_open()) {
			cerr << "fail to open file " << meta_data.libs << endl;
			exit(1);
		}
		if (!input.good()) {
			input.close();
			cerr << meta_data.libs << " file is broken" << endl;
			exit(1);
		}
		string proc_name;
		while(getline(input, proc_name)) {
			cerr << "load libinfo for " << proc_name << endl;
			if (!LoadData::load_lib(proc_name))
				cerr << "Fail to load lib for " << proc_name << endl;
		}
		input.close();
	}
	cerr << "Regnerated lib info" << endl;

	time_t time_begin, time_end;
	/* parser */
	cout << "Begin parse..." << endl;
	time(&time_begin);
	lldb::SBDebugger::Initialize();
	LoadData::preload();
	event_lists = Parse::divide_and_parse();
	EventLists::sort_event_list(event_lists[0]);
	lldb::SBDebugger::Terminate();
	time(&time_end);
	cout << "Time cost for parsing ";
	cout << fixed << setprecision(1) << difftime(time_end, time_begin) << "seconds"<< endl;
	return 0;
}

void EventLists::sort_event_list(list<event_t *> &evlist)
{
	evlist.sort(Parse::EventComparator::compare_time);
}

void EventLists::dump_all_event_to_file(string filepath)
{
	ofstream dump(filepath);
	if (dump.fail()) {
		cout << "unable to open file " << filepath << endl;
		exit(EXIT_FAILURE);
	}

	list<event_t *> &evlist = event_lists[0];
	list<event_t *>::iterator it;
	for(it = evlist.begin(); it != evlist.end(); it++) {
		event_t *cur_ev = *it;
		cur_ev->decode_event(1, dump);
	}
	dump.close();
}

void EventLists::streamout_all_event(string filepath)
{
	ofstream dump(filepath);
	if (dump.fail()) {
		cout << "unable to open file " << filepath << endl;
		exit(EXIT_FAILURE);
	}
	list<event_t *> &evlist = event_lists[0];
	list<event_t *>::iterator it;
	for(it = evlist.begin(); it != evlist.end(); it++) {
		event_t *cur_ev = *it;
		cur_ev->streamout_event(dump);
	}
	dump.close();
}

void EventLists::streamout_backtrace(string filepath)
{
	ofstream dump(filepath);
	if (dump.fail()) {
		cout << "unable to open file " << filepath << endl;
		exit(EXIT_FAILURE);
	}

	list<event_t *> &evlist = event_lists[BACKTRACE];
	list<event_t *>::iterator it;
	for(it = evlist.begin(); it != evlist.end(); it++) {
		event_t *cur_ev = *it;
		if (dynamic_cast<backtrace_ev_t *>(cur_ev))
			cur_ev->streamout_event(dump);
	}
	dump.close();
}

void EventLists::clear_event_list()
{
	list<event_t *> &evlist = event_lists[0];
	list<event_t *>::iterator it;
	for(it = evlist.begin(); it != evlist.end(); it++) {
		event_t *cur_ev = *it;
		delete(cur_ev);
	}
	evlist.clear();
	event_lists.clear();
}
