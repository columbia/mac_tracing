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
#include "command.h"

class ArgusLoop : public CompositeCommand {
public:
    EventGraph *event_graph = nullptr;
    TransactionGraph *trans_graph = nullptr;
    ImportedGraph *import_graph = nullptr;
    GraphSearcher *search_engine = nullptr;

    ArgusLoop() : CompositeCommand("", ""),
                event_graph(nullptr),
                trans_graph(nullptr),
                import_graph(nullptr),
                search_engine(nullptr) {}
	void register_interactive_debugger_commands();
	void register_path_commands();
	void register_graph_commands();
    ~ArgusLoop();
    virtual void invokeNull(Arguments args) {}
    void invokeDefault(Arguments args) {
        std::cout << "unknown command, try \"help\"\n";
    }
    std::string readline(const std::string &prompt);
};

void mainloop(ArgusLoop &commands);

#endif
