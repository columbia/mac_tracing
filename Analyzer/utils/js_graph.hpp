#ifndef JS_GRAPH_HPP
#define JS_GRAPH_HPP
#include "graph.hpp"
class JSGraph{
    Graph *graph;
    std::vector<Node *> nodes;
    /* dump as javascript */
private:
    void js_edge(std::ofstream &outfile, EventBase *host, const char *action, EventBase *peer, bool *comma);
    void message_edge(std::ofstream &outfile, EventBase *event, bool *);
    void dispatch_edge(std::ofstream &outfile, EventBase *event, bool *);
    void timercall_edge(std::ofstream &outfile, EventBase *event, bool *);
    void mkrun_edge(std::ofstream &outfile, EventBase *event, bool *);
    void wait_label(std::ofstream &outfile, EventBase *event, bool *);
    void coreannimation_edge(std::ofstream &outfile, EventBase *event, bool *);

    void js_lanes(std::vector<Node *>&, std::ofstream &outfile);
    void js_groups(std::vector<Node *>&, std::ofstream &outfile);
    void js_arrows(std::vector<Node *>&, std::ofstream &outfile);
public:
    JSGraph(Graph *_graph);
    void js_cluster(std::vector<Node *>&, std::ofstream &outfile);
    void js_cluster(std::string path, Node *);
    
};

#endif
