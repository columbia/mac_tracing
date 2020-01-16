#include <iostream>
#include <iomanip>
#include <sstream>
#include <dirent.h>

#include "argus_loop.h"
#include "critical_path.hpp"
#include "wait_check_path.hpp"

ArgusLoop::~ArgusLoop()
{
    if (search_engine != nullptr) {
        delete search_engine;
        search_engine = nullptr;
    }

    if (trans_graph != nullptr) {
        delete trans_graph;
        trans_graph = nullptr;
    }

    if (import_graph != nullptr) {
        delete import_graph;
        import_graph = nullptr;
    }

    if (event_graph != nullptr) {
        delete event_graph;
        event_graph = nullptr;
    }
}

void ArgusLoop::register_interactive_debugger_commands()
{
	ArgusLoop &commands = *this;
    this->add("debug-on-graph", [&](Arguments args) {
            Graph *graph = nullptr;
            args.shouldHave(1);
            switch (args.get(0).c_str()[0]) {
                case '0' : graph = commands.event_graph;
                            break;
                case '1' : graph = commands.trans_graph;
                            break;
                case '2' : graph = commands.import_graph;
                            break;
                default: return;
            }

            if (graph == nullptr) {
                std::cout << "No graph avialable" << std::endl;
                return;
            }
            if (commands.search_engine) {
                delete commands.search_engine;
                commands.search_engine = nullptr;
            }
            commands.search_engine = new GraphSearcher(graph);
            uint64_t stopped_gid = commands.search_engine->init_diagnose();
            std::cout << "Path slice stops in node with id 0x"\
            << std::hex << stopped_gid << std::endl;
        }, "begin interactive debugging");

    /*break downs of debuger on interactive path searching*/
    this->add("debugger-detect-spinning", [&](Arguments args) {
            args.shouldHave(0);
            if (!commands.search_engine || LoadData::meta_data.host == "Undefined")
				return;
            uint64_t group_id = commands.event_graph->get_spinning_node()->get_gid();
            std::cout << "Main thread of " << LoadData::meta_data.host << " is in 0x"\
                << std::hex << group_id << "\nSpinning type is " \
                << commands.search_engine->decode_spinning_type() << std::endl; 
        }, "get the spinning node id");

    this->add("debugger-clear-path", [&](Arguments args) {
            if (commands.search_engine)
            commands.search_engine->clear_path();
            else
            std::cout << "No debugger inited\n";
        }, "clear path from search engine");

    this->add("debugger-clear-path-from", [&](Arguments args) {
            args.shouldHave(1);
            uint64_t gid;
            args.asHex(0, &gid);
            if (commands.search_engine)
            commands.search_engine->clear_path_from_node(gid);
            else
            std::cout << "No debugger inited\n";
         }, "clear partial path, from given node to the last");

    this->add("debugger-search-from", [&](Arguments args) {
            if (commands.search_engine == nullptr) {
                std::cout << "No debugger inited\n";
                return;
            }
            args.shouldHave(2); 
            uint64_t gid, index;
            args.asHex(0, &gid);
            args.asHex(1, &index);
            uint64_t stopped_gid = commands.search_engine->continue_backward_slice(gid, index);
            std::cout << "Path slice stops in node with id 0x"\
                << std::hex << stopped_gid << std::endl;
        }, "begin search with input node id and event sequence in hex");

    this->add("debugger-thread-compare", [&](Arguments args) {
            if (commands.search_engine == nullptr) return;
            args.shouldHave(0); 
            if (commands.search_engine->is_wait_spinning() == false)
                std::cout << "Nothing to compare" << std::endl;
            commands.search_engine->path_comparison();
        }, "begin compare normal case with buggy case");

    this->add("debugger-print-path", [&](Arguments args) {
            if (commands.search_engine == nullptr)
                return;
            commands.search_engine->show_path();
        }, "show the sliced path");
}

void ArgusLoop::register_graph_commands()
{
	ArgusLoop &commands = *this;
    this->add("set-host-proc", [&](Arguments args) {
            args.shouldHave(1);
            std::string appname = args.get(0);
            LoadData::meta_data.host = appname;
            LoadData::symbolic_procs[appname] = 1;
        }, "Inspect the performance issue for the process");

    this->add("set-live-pid", [&](Arguments args) {
            uint64_t set_pid;
            args.shouldHave(1);
            if (args.asDec(0, &set_pid))
                LoadData::meta_data.pid = (pid_t) set_pid;
        }, "Set live process id for correcting CoreGraphics lldb-symbolication");

    this->add("set-symbolic-proc", [&](Arguments args) {
            args.shouldHave(1);
            std::string procname = args.get(0);
            LoadData::symbolic_procs[procname] = 1;
        }, "Request callstack event symbolication for a process");

    this->add("construct-event-graph", [&](Arguments args) {
            args.shouldHave(1);
            std::string logfile = args.get(0);
            LoadData::meta_data.data = logfile;
            if (!LoadData::meta_data.host.size()
                || access(logfile.c_str(), R_OK) != 0)
            return;
            if (commands.event_graph != nullptr) {
                delete(commands.event_graph);
                commands.event_graph = nullptr;
            }
            commands.event_graph = new EventGraph();
       }, "construct event graph with log file");

    this->add("construct-trans-graph", [&](Arguments args) {
            args.shouldHave(1);
            uint64_t gid;
            args.asHex(0, &gid);
            if (!commands.event_graph 
                || commands.event_graph->id_to_node(gid) == nullptr) {
            	std::cout << "no event graph" << std::endl;
            	return;
            }
            if (commands.trans_graph) {
                delete commands.trans_graph;
                commands.trans_graph = nullptr;
            }
            commands.trans_graph = new TransactionGraph(commands.event_graph->get_groups_ptr(), gid);
        }, "construct transaction graph with root node id");

    this->add("construct-imported-graph", [&](Arguments args) {
            args.shouldHave(1); 
            if (commands.import_graph) {
                delete commands.import_graph;
                commands.import_graph = nullptr;
            }
            commands.import_graph = new ImportedGraph(commands.event_graph, args.get(0));
        }, "import graph edges from edge file");

    this->add("export-graph-for-ml", [&](Arguments args) {
            args.shouldHave(2);
            std::string logpath = args.get(1);
            Graph *graph = nullptr;
            switch (args.get(0).c_str()[0]) {
                case '0' : graph = commands.event_graph;
                            break;
                case '1' : graph = commands.trans_graph;
                            break;
                case '2' : graph = commands.import_graph;
                            break;
                default: return;
            }
            if (graph == nullptr)
                return;
            graph->tfl_nodes_and_edges(logpath);
        }, "store graph to file [graph-type] [log-path]");

	this->add("save-graph", [&](Arguments args) {
            args.shouldHave(2);
            std::string logpath = args.get(1);
            Graph *graph = nullptr;
            switch (args.get(0).c_str()[0]) {
                case '0' : graph = commands.event_graph;
                            break;
                case '1' : graph = commands.trans_graph;
                            break;
                case '2' : graph = commands.import_graph;
                            break;
                default: return;
            }
            if (graph == nullptr)
                return;
            graph->streamout_nodes_and_edges(logpath);
		}, "save graph to file [graph-type] [log-path]");
    
    this->add("graph-types", [&](Arguments args) {
                std::cout << "0\tevent_graph";
				if (commands.event_graph) std::cout << "\texist";
				std::cout << std::endl;
                std::cout << "1\ttrans_graph";
				if (commands.trans_graph) std::cout << "\texist";
				std::cout << std::endl;
                std::cout << "2\timport_graph";
				if (commands.import_graph) std::cout << "\texist";
				std::cout << std::endl;
        }, "list supported graph types");

    this->add("grep-event", [&](Arguments args){
            std::string timestamp = args.get(0);
            std::string type = args.get(1);
            std::string procname = args.get(2);
            uint64_t tid;
            args.asHex(3, &tid);
            commands.event_graph->get_event_lists()->show_events(timestamp, type, procname, tid);    
        }, "grep events from event graph [time_stamp] [type] [proc] [tid]");

    this->add("show-node", [&](Arguments args) {
            if (commands.event_graph == nullptr) return;
            args.shouldHave(1); 
            uint64_t gid;
            args.asHex(0, &gid);
            std::cout << "show-node 0x" << std::hex << gid << std::endl;
            Node *node = commands.event_graph->id_to_node(gid);
            if (node != nullptr) {
                node->get_group()->pic_group(std::cout);
                node->get_group()->streamout_group(std::cout);
            }
        }, "Rendering node information to file /tmp/argus_node_info");

}

void ArgusLoop::register_path_commands()
{
	ArgusLoop &commands = *this;

	this->add("extract-critical-path", [&](Arguments args) {
			args.shouldHave(3);
			uint64_t gid_from, gid_to;
			args.asHex(0, &gid_from);
			args.asHex(1, &gid_to);
			std::string output_path = args.get(2);
			Node *from = commands.event_graph->id_to_node(gid_from);
			Node *to = commands.event_graph->id_to_node(gid_to);
			if (from == nullptr || to == nullptr) {
				std::cout << "invalid nodes" << std::endl;
				return;
			}
			CriticalPath(commands.event_graph, from, to, output_path);
	    }, "check path from arg1 to arg2 in event graph");
	
	this->add("extract-main-path", [&](Arguments args) {
			args.shouldHave(4);
            //char *ch_end;
            //uint64_t gid_from = std::strtoull(args.get(0).c_str(), &ch_end, 16);
            //uint64_t gid_to = std::strtoull(args.get(2).c_str(), &ch_end, 16);
			uint64_t gid_from, index_from, gid_to, index_to;
			args.asHex(0, &gid_from);
			args.asHex(1, &index_from);
			args.asHex(2, &gid_to);
			args.asHex(3, &index_to);
			std::cout << std::hex << "0x" << gid_from << "\t0x" << index_from << std::endl;
			std::cout << std::hex << "0x" << gid_to << "\t0x" << index_to << std::endl;
			Node *node_from = commands.event_graph->id_to_node(gid_from);
			Node *node_to = commands.event_graph->id_to_node(gid_to);
			assert(node_from);
			assert(node_to);
			EventBase *begin = node_from->index_to_event(index_from);
			EventBase *end = node_to->index_to_event(index_to);
			assert(begin != nullptr);
			assert(end != nullptr);
			FillPath fillpath(commands.event_graph, begin, end);
			std::string path("./output/main_path");
			path += std::to_string(gid_from);
			path += "_";
			path += std::to_string(gid_to);
			path += ".log";
			fillpath.save_path_to_file(path); 
	    }, "check path from gid_arg1, index_arg2 to gid_arg3, index_arg4 in event graph");
	
	this->add("export_edge_tfl", [&](Arguments args) {
			args.shouldHave(4);
			uint64_t from, to;
			args.asDec(0, &from);
			args.asDec(1, &to);
			std::string weak_edge_path = args.get(2);
			std::string output_path = args.get(3);
			commands.event_graph->tfl_edges_between(from, to, weak_edge_path, output_path);
		}, "export edgs between two events' tfl index");

	this->add("tfl_index2event", [&](Arguments args) {
			args.shouldHave(1);
			uint64_t index;
			args.asDec(0, &index);
			commands.event_graph->show_event_info(index, std::cout);
		}, "show event information for the index of event");
}

std::string ArgusLoop::readline(const std::string &prompt)
{
    std::string line;
    std::cout << prompt;
    std::cout.flush();
    std::getline(std::cin, line);
    return line;
}

static void printUsageHelper(Command *command, int level);

static void printUsage(ArgusLoop *command) {
    std::cout << "usage:\n";
    for(auto c : command->getMap()) {
        printUsageHelper(c.second, 1);
    }
}

static void printUsageHelper(Command *command, int level) {
    for(int i = 0; i < level; i ++) std::cout << "    ";

    std::cout << std::left << std::setw(30) << command->getName()
        << " " << command->getDescription() << std::endl;

    if(auto composite = dynamic_cast<CompositeCommand *>(command)) {
        for(auto c : composite->getMap()) {
            printUsageHelper(c.second, level + 1);
        }
    }
}

void mainloop(ArgusLoop &commands)
{
    bool running = true;
    //ArgusLoop commands;
    commands.add("quit", [&](Arguments) {running = false;}, "quit");

    commands.add("help", [&](Arguments) {
            printUsage(&commands); },"print help info");

    commands.add("ls", [&](Arguments args) {
            args.shouldHave(1); 
            std::string dir_path = args.get(0);
            DIR *dir;
            struct dirent *ent;
            if ((dir = opendir (dir_path.c_str())) != NULL) {
                while ((ent = readdir (dir)) != NULL)
                    std::cout << ent->d_name <<"\t";
                std::cout << std::endl;
                closedir (dir);
            } else {
                std::cout << dir_path << " : path doesn't exist" << std::endl;
            }
        }, "show files in directory");

	commands.register_graph_commands();
	commands.register_path_commands();

    while (running) {
        std::string line = commands.readline("argus> ");
        std::istringstream sstream(line);
        Arguments args;
        std::string arg;
        std::string comp_arg;
        while (sstream >> arg) {
            if (arg.back() != '\\') {
                comp_arg += arg;
                args.add(comp_arg);
                comp_arg.clear();
            } else {
                arg.back() = ' ';
                comp_arg += arg;
            }
        }
        if (comp_arg.size() > 0)
            args.add(comp_arg);

        if(args.size() > 0) {
            try {
                commands(args);
            }
            catch(const char *s) {
                std::cout << "error: " << s << std::endl;
            }
            catch(const std::string &s) {
                std::cout << "error: " << s << std::endl;
            }
        }
    }
	std::cout << "Exit argus_shell" << std::endl;
}

