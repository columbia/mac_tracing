#ifndef ARGUS_LOOP_HPP
#define ARGUS_LOOP_HPP
#include <iostream>
#include <unistd.h>
#include <boost/filesystem.hpp>

#include "parser.hpp"
#include "group.hpp"
#include "canonization.hpp"
#include "graph.hpp"
#include "import_graph.hpp"
#include "search_graph.hpp"
#include "critical_path.hpp"
#include "tfl_graph.hpp"
#include "command.h"

class ArgusLoop : public CompositeCommand {
public:
    EventGraph *event_graph;
    TransactionGraph *trans_graph;
    ImportedGraph *import_graph;
    GraphSearcher *search_engine;
	TraceForLearn *tfl_graph;

    ArgusLoop() : CompositeCommand("", ""),
                event_graph(nullptr),
                trans_graph(nullptr),
                import_graph(nullptr),
                search_engine(nullptr),
				tfl_graph(nullptr) {}
	void register_interactive_debugger_commands();
	void register_path_commands();
	void register_graph_commands();
	void register_tfl_commands();
    ~ArgusLoop();
	virtual void invokeNull(Arguments args) {}
	void invokeDefault(Arguments args) {
		std::cout << "unknown command, try \"help\"\n";
	}
	void invoke_lldb_script();
	std::string readline(const std::string &prompt);
};

void mainloop(ArgusLoop &commands);

#endif
