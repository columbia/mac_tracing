#include "parser.hpp"
#include "group.hpp"
#include "canonization.hpp"
//#include "cluster.hpp"
#include "graph.hpp"
//#include "search_engine.hpp"
#include "search_graph.hpp"
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
	cerr << "Signal " << sig << endl;
	backtrace_symbols_fd(btarray, size, STDERR_FILENO);
	mtx.unlock();
	exit(1);
}

void check_search_engine(GraphSearcher *search_engine, Node *spinning_node)
{

	assert(spinning_node != NULL);
	cout << "Spinning data " << spinning_node->get_group()->get_group_id() << endl;

	vector<Node *> similar_nodes = search_engine->get_similar_nodes_in_thread_before(spinning_node);
	if (similar_nodes.size() == 0) {
		similar_nodes = search_engine->get_similar_nodes_in_thread_before(spinning_node, 4);
	}

	vector<Node *>::iterator it;
	cout << "similar node " << similar_nodes.size() << endl;
	for (it = similar_nodes.begin(); it != similar_nodes.end(); it++)
		cout << "Similar group " << (*it)->get_group()->get_group_id() << endl;

	//vector<Node *> counterparts = search_engine->get_counterpart_on_syscall(spinning_node, similar_nodes);
	//cout << "Counter node " << counterparts.size() << endl;
	vector<Node *> counterparts = similar_nodes;
	for (it = counterparts.begin(); it != counterparts.end(); it++)
		cout << "Counter group " << (*it)->get_group()->get_group_id() << endl;

	if (counterparts.size()  > 0) {
		Node *normal_node = counterparts.back();
		cout << "Check normal node " << normal_node->get_group()->get_group_id() << endl;
	
		vector<Node *>candidates = search_engine->wakeup_node(normal_node);
		if (candidates.size() > 0) {
			cout << "Candidate size " << candidates.size() << endl;
			vector<Node *> norm_path = search_engine->slice_path_for_node(candidates.back(), false);
			for (it = norm_path.begin(); it != norm_path.end(); it++)
	    			cout << "Group in normpath " << (*it)->get_group()->get_group_id() << endl;
			if (norm_path.size() >0) {
				search_engine->examine_path_backward(spinning_node, norm_path, false);
			}
		}
	}
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

	cout << "Check graphs..." << endl; 
	string graph_stream_path = get_prefix(logfile) + "_graph.stream";
	Graph *graph = new Graph(g_ptr);

////////////////////////////////////////////////////
// implementation of analysis toolkit below       //
// can be improved with interface for above       //
////////////////////////////////////////////////////
	map<event_id_t, bool> key_events;
	key_events.clear();

	cout << "Construct search engine..." << endl;
	GraphSearcher *search_engine = new GraphSearcher(graph, key_events);

	vector<Node *>::iterator it;
	Node *spinning_node = search_engine->spinning_node();

	if (spinning_node == NULL) {
		cout << "No spinning node found" << endl;
		goto out;
	}

	check_search_engine(search_engine, spinning_node);
	//graph->streamout_nodes_and_edges(graph_stream_path);

out:
	cout << "Clearing toolkit..." << endl;
	delete search_engine;

	cout << "Clearing graph..." << endl;
	delete graph;

	cout << "Clearing groups..." << endl;
	delete g_ptr;

	cout << "Clearing events..." << endl;
	delete event_lists_ptr;

	cout << "Done!" << endl;
	return 0;
}
