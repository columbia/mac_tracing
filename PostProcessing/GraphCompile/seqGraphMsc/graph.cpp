#include "graph.hpp"

Edge::Edge(float _timestamp, uint64_t _tid, string _from, 
			string _color, int _gid, string _action, uint64_t _peer_tid)
{
	timestamp = _timestamp;

	tid = _tid;
	from = _from;
	from.append(to_string(_tid));
	
	color = _color;
	label = to_string(_gid);
	action = _action;
	
	peer_tid = _peer_tid;
	to.clear();
}

bool Edge::update_peernode(map<uint64_t, string> &_tid_proc_mapping)
{
	if (peer_tid != 0 && _tid_proc_mapping.find(peer_tid) != _tid_proc_mapping.end()) {
		to = _tid_proc_mapping[peer_tid];
		return true;
	}
	return false;
}

Graph::Graph(string _name)
{
	name = _name;
	edges.clear();
	nodes.clear();
	tid_proc_mapping.clear();
}

void Graph::add_node(string node)
{
	if(nodes.find(node) != nodes.end())
		return;
	nodes[node] = nodes.size();
}

void Graph::add_edge(Edge e)
{
	edges.push_back(e);
	add_node(e.from);
	tid_proc_mapping[e.tid] = e.from;
}

void Graph::print_nodes()
{
	ofstream outfile(name);
	outfile << "msc {\n";
	map<string, int>::iterator it;
	bool comma = false;
	for(it = nodes.begin(); it != nodes.end(); it++) {
		if (comma)
			outfile << ",\n";
		else
			comma = true;
		outfile << it->first << " [label=\"" << it->first;
		outfile << "\", textbgcolor = \"orange\"]";
	}
	outfile <<";\n";
	outfile.close();
}

void Graph::print_edges()
{
	list<Edge>::iterator it;
	ofstream outfile;
	outfile.open(name, ofstream::out | ofstream::app);

	bool begin = false;
	string actions;
	list<string> cur_edges;
	string cur_gid_label;
	string cur_edge;
	string cur_color;

	for (it = edges.begin(); it != edges.end(); it++) {
		Edge e = *it;
		e.update_peernode(tid_proc_mapping);
		if (begin == false) {
			begin = true;
			actions.clear();
			cur_edges.clear();
			cur_gid_label = e.label;
			cur_edge = e.from;
			cur_color = e.color;
		} 
		
		if (e.action.find("backtrace") != string::npos) {
			if (actions.length() > 0) {
				outfile << e.from << " abox " << e.from << " [label=";
				outfile << "\"" << cur_gid_label << "\\n" << actions << "\", textbgcolor=\"" << e.color << "\"];\n";
			}

			outfile << e.from << " note " << e.from << "[label=\"" << e.action << "\", textbgcolour=\"" << e.color << "\"];\n";
			begin = false;
			actions.clear();
		} else {
			if (cur_gid_label == e.label)
				actions.append(e.action+"\\n");
			else {
				outfile << cur_edge << " abox " << cur_edge << " [label=";
				outfile << "\"" << cur_gid_label << "\\n" << actions << "\", textbgcolor=\"" << cur_color << "\"];\n";
				begin = false;
				actions.clear();
				cur_edges.clear();
				cur_gid_label = e.label;
				cur_edge = e.from;
				cur_color = e.color;
			}
		}
	}
	outfile << "}\n";
	outfile.close();
}



