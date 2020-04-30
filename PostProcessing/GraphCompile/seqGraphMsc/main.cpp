#include "graph.hpp"
#include <sstream>
#include <iostream>
using namespace std;

string color_array[] = {
				"blue", "aquamarine", "burlywood", "cadetblue", "cornflowerblue",
				"darkgrey" , "darkgreen", "darkorange", "mediumpurple", "gold",
				"forestgreen", "sienna", "yellowgreen"};

string space2underscore(string text) {
    for(std::string::iterator it = text.begin(); it != text.end(); ++it) {
        if(*it == ' ') {
            *it = '_';
        }
    }
    return text;
}

int main(int argc, char** argv)
{
	if (argc < 3)
		cout << "Missing input file or output dir" << endl;
	ifstream infile(argv[1]);
	string line;

	Graph * graph = NULL;
	bool processing = false;
	while(getline(infile, line)) {
		if (line.find("Cluster") != string::npos) {
			if (line.find("103") != string::npos) {
				processing = true;
			} else {
				processing = false;
			}
			if (graph != NULL) {
				graph->print_nodes();
				graph->print_edges();
				graph->edges.clear();
				graph->nodes.clear();
				delete(graph);
				graph = NULL;
			}
			
			if (processing == true) {
				string path(argv[2]);
				path.append("/");
				size_t pos = line.find("(");
				string filename = line.substr(1, pos);
				filename = space2underscore(filename);
				path.append(filename);
				path.append(".mscin");
				graph = new Graph(path);
			}
			continue;
		} 
		if (processing == false)
			continue;

		int gid;
		float abstime;
		uint64_t tid;
		string proc;
		string action;
		uint64_t peer_tid;
		istringstream iss(line);
		if (!(iss >> hex >> gid >> abstime >> hex >> tid >> proc >> action))
			break;
		if (!(iss >> hex >> peer_tid))
			peer_tid = 0;
		Edge e(abstime, tid, proc, color_array[gid%13], gid, action, peer_tid);
		graph->add_edge(e);
	}
	return 0;
}
