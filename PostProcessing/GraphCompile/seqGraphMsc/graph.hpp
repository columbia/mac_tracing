#ifndef GRAPH__HPP
#define GRAPH__HPP
#include <map>
#include <vector>
#include <list>
#include <string>
#include <fstream>

using namespace std;

class Edge {
public:
	float timestamp;
	uint64_t tid;
	string from;
	string color;
	string label;
	string arrow;
	uint64_t peer_tid;
	string to;
	string action;
	Edge(float, uint64_t, string, string, int, string, uint64_t);
	bool update_peernode(map<uint64_t, string> & tid_proc_mapping);
};

class Graph {
public:
	string name;
	list<Edge> edges;
	map<string, int> nodes;
	map<uint64_t, string> tid_proc_mapping;
	Graph(string _name);
	void add_node(string node);
	void add_edge(Edge e);
	void print_nodes();
	void print_edges();
};

/*
static vector<string> color_array = {
				"blue", "aquamarine", "burlywood", "cadetblue", "cornflowerblue",
				"darkgrey" , "darkgreen", "darkorange", "mediumpurple", "gold",
				"forestgreen", "sienna", "yellowgreen"};
*/
/*
class PlotActivity {
	string input;
	string output_dir;
	static vector<string> color_array;
	map<uint64_t, string> tid_proc_mapping;
	PlotActivity(string _input, string _output_dir);
	void process();
}
*/

#endif
