#include <iostream>
#include <iomanip>
#include <sstream>
#include <dirent.h>

#include "argus_loop.h"
#include "critical_path.hpp"
#include "wait_path.hpp"
#include "weak_edges_estimate.hpp"
#include "classify_vertices.hpp"

#if defined(__APPLE__)
#include <Python.h>
#endif

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
	
	if (tfl_graph != nullptr) {
		delete tfl_graph;
		tfl_graph = nullptr;
	}

    if (event_graph != nullptr) {
        delete event_graph;
        event_graph = nullptr;
    }
}

void ArgusLoop::register_interactive_debugger_commands()
{
	ArgusLoop &commands = *this;
	CompositeCommand *debugger = new CompositeCommand("debugger", "interactive debugging");
	this->add(debugger);

    debugger->add("init-on-graph", [&](Arguments args) {
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
            LOG_S(INFO) << "Path slice stops in node with id 0x"\
            	<< std::hex << stopped_gid << std::endl;
        }, "begin interactive debugging");

    /*break downs of debuger on interactive path searching*/
    debugger->add("detect-spinning-cursor", [&](Arguments args) {
            args.shouldHave(0);
            if (!commands.search_engine || LoadData::meta_data.host == "Undefined")
				return;
            uint64_t group_id = commands.event_graph->get_spinning_node()->get_gid();
            std::cout << "Main thread of " << LoadData::meta_data.host << " is in 0x"\
                << std::hex << group_id << "\nSpinning type is " \
                << commands.search_engine->decode_spinning_type() << std::endl; 
        }, "get the spinning node id, should have search engine and host set");

	debugger->add("set-problem-vertex", [&](Arguments args) {
            args.shouldHave(1);
            if (!commands.search_engine || LoadData::meta_data.host == "Undefined")
				return;
			commands.search_engine->set_anomaly_node(args.get_num(0, "hex"));
		}, "user indicate the problem vertex");
	
	debugger->add("baseline-to", [&](Arguments args) {
			args.shouldHave(1);
            if (!commands.event_graph || !commands.search_engine || LoadData::meta_data.host == "Undefined") {
				std::cout << "Invalid settings" << std::endl;
				return;
			}

			uint64_t gid = args.get_num(0, "hex");
            Node *node = commands.event_graph->id_to_node(gid);
			if (!node) {
				std::cout << "Invalid spinning node id" << std::endl;
				return;
			}
			LOG_S(INFO)<< "Similar nodes to 0x" << std::hex << gid << std::endl;
			for (auto baseline : commands.search_engine->search_baseline_nodes(node))
				LOG_S(INFO)<< "0x" << std::hex << baseline->get_gid() << std::endl;

		}, "get the baseline nodes to spinning node, should have search engine and host set");

	debugger->add("search-from-baseline", [&](Arguments args) {
            args.shouldHave(1);
			uint64_t gid = args.get_num(0, "hex");
			if (!commands.search_engine)
				return;
			commands.search_engine->search_from_baseline(gid);

		}, "user indicate the baseline scenario vertex for comparison");

    debugger->add("clear-path", [&](Arguments args) {
            args.shouldHave(0);
            if (commands.search_engine)
            	commands.search_engine->clear_path();
            else
            	std::cout << "No debugger inited\n";
        }, "clear path from search engine");

    debugger->add("shrink-path", [&](Arguments args) {
            args.shouldHave(1);
			uint64_t gid = args.get_num(0, "hex");
			if (commands.search_engine)
				commands.search_engine->clear_path_from_node(gid);
			else
				std::cout << "No debugger inited\n";
         }, "delete nodes in the path, from given node to the last");

    debugger->add("search-from", [&](Arguments args) {
            args.shouldHave(2); 
            if (commands.search_engine == nullptr) {
                std::cout << "No debugger inited\n";
                return;
            }
			uint64_t gid = args.get_num(0, "hex");
			uint64_t index = args.get_num(1, "hex");

            uint64_t stopped_gid = commands.search_engine->continue_backward_slice(gid, index);
            std::cout << "Path slice stops in node with id 0x"\
                << std::hex << stopped_gid << std::endl;
        }, "begin search with input node and event index in hex");

    debugger->add("thread-compare", [&](Arguments args) {
            if (commands.search_engine == nullptr)
				return;
            args.shouldHave(0); 
            if (commands.search_engine->is_wait_spinning() == false)
                std::cout << "Nothing to compare" << std::endl;
            commands.search_engine->path_comparison();
        }, "begin compare normal case with buggy case");

   	debugger->add("print-path", [&](Arguments args) {
            if (commands.search_engine == nullptr)
                return;
            commands.search_engine->show_path();
        }, "show the sliced path");

	debugger->add("record-selection", [&](Arguments args) {
            if (commands.search_engine == nullptr)
                return;
            commands.search_engine->record_selection();
		}, "record user-interaction");

#if defined(__APPLE__)
	debugger->add("add-info-for-range", [&](Arguments args) {
			args.shouldHave(2);
			std::string api_from = args.get(0);
			std::string api_to = args.get(1);
			//TODO: python plugins from file		
			invoke_lldb_script();
		}, "intergrate more informatin from arg1(API) to arg2(API)");
#endif

	debugger->add("print-root-causes", [&](Arguments args) {
			//spinning_node: if busy, show callstacks and UI
			//similar_node : else
			//suspect_node : else
		}, "export root causes diagnosized to file");
	
	debugger->add("similar-nodes-to", [&](Arguments args) {
			args.shouldHave(1);
			if (!commands.search_engine) {
				LOG_S(INFO) << "No debugger inited, and init a search engine" << std::endl;
            	commands.search_engine = new GraphSearcher(commands.event_graph);
			}
			commands.search_engine->show_similar_nodes(args.get_num(0, "hex"));
		}, "find similar node to input node");

	debugger->add("save-similar-node-pair", [&](Arguments args) {
			args.shouldHave(2);
			if (!commands.search_engine) {
				LOG_S(INFO) << "No debugger inited" << std::endl;
				return;
			}
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			commands.tfl_graph->tfl_pair_to_file(args.get_num(0, "hex"),
				args.get_num(1, "hex"), "output/positive_similar.log");
			
		}, "save positive pairs to file");

	debugger->add("save-unsimilar-node-pair", [&](Arguments args) {
			args.shouldHave(2);
			if (!commands.search_engine) {
				LOG_S(INFO) << "No debugger inited" << std::endl;
				return;
			}
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			commands.tfl_graph->tfl_pair_to_file(args.get_num(0, "hex"),
				args.get_num(1, "hex"), "output/negative_similar.log");
			
		}, "save negative pairs to file");
}

void ArgusLoop::register_graph_commands()
{
	ArgusLoop &commands = *this;
	CompositeCommand *config = new CompositeCommand("set", "config with system info");
	this->add(config);
	CompositeCommand *construct = new CompositeCommand("construct", "construct graphs");
	this->add(construct);
	CompositeCommand *view = new CompositeCommand("view", "check current diagnositics");
	this->add(view);

    config->add("host-proc", [&](Arguments args) {
            args.shouldHave(1);
            std::string appname = args.get(0);
            LoadData::meta_data.host = appname;
            LoadData::symbolic_procs[appname] = 1;
        }, "Inspect the performance issue for the process");

    config->add("live-pid", [&](Arguments args) {
            uint64_t set_pid;
            args.shouldHave(1);
            if (args.asDec(0, &set_pid))
                LoadData::meta_data.pid = (pid_t) set_pid;
        }, "Set any live GUI app pid for correcting lldb-symbolication for CoreGraphics");

    config->add("symbolize-proc", [&](Arguments args) {
            args.shouldHave(1);
            std::string procname = args.get(0);
            LoadData::symbolic_procs[procname] = 1;
        }, "Request callstack event symbolication for a process");

    construct->add("event-graph", [&](Arguments args) {
            args.shouldHave(1);
            std::string logfile = args.get(0);
            LoadData::meta_data.data = logfile;
			
			LOG_S(INFO) << "host : " << LoadData::meta_data.host << std::endl;

			if (access(logfile.c_str(), R_OK) != 0)
            	return;
            if (commands.event_graph != nullptr) {
                delete(commands.event_graph);
                commands.event_graph = nullptr;
            }
            commands.event_graph = new EventGraph();
       }, "construct event graph with log file");

    construct->add("trans-graph", [&](Arguments args) {
            args.shouldHave(1);
			uint64_t gid = args.get_num(0, "hex");

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

    
    view->add("graphs", [&](Arguments args) {
                std::cout << "0\tevent_graph";
				if (commands.event_graph) std::cout << "\texist";
				std::cout << std::endl;
                std::cout << "1\ttrans_graph";
				if (commands.trans_graph) std::cout << "\texist";
				std::cout << std::endl;
                std::cout << "2\timport_graph";
				if (commands.import_graph) std::cout << "\texist";
				std::cout << std::endl;
        }, "list supported existing graphs");

    view->add("events", [&](Arguments args){
            std::string timestamp = args.get(0);
            std::string type = args.get(1);
            std::string procname = args.get(2);
			uint64_t tid = args.get_num(3, "hex");

            commands.event_graph->get_event_lists()->show_events(timestamp, type, procname, tid);    
        }, "events from event graph with [time_stamp] [type] [proc] [tid]");

    view->add("vertex", [&](Arguments args) {
            args.shouldHave(1); 
            if (commands.event_graph == nullptr)
				return;
			uint64_t gid = args.get_num(0, "hex");

            std::cout << "show-node 0x" << std::hex << gid << std::endl;
            Node *node = commands.event_graph->id_to_node(gid);
            if (node != nullptr) {
                node->get_group()->pic_group(std::cout);
                node->get_group()->streamout_group(std::cout);
            }
        }, "Rendering vertex information");
	/*
	view->add("similar-vertices", [&](Arguments args) {
			args.shouldHave(1);
			if (!commands.search_engine) {
            	commands.search_engine = new GraphSearcher(commands.event_graph);
			}
			commands.search_engine->show_similar_nodes(args.get_num(0, "hex"));
		}, "Find similar node to the input node id"); 
	*/
	
	view->add("event-at", [&](Arguments args) {
			if (commands.event_graph == nullptr)
				return;
			args.shouldHave(2);
			uint64_t gid = args.get_num(0, "hex");
			uint64_t index = args.get_num(1, "hex");
			std::cout << "show-event 0x"\
				<< std::hex << gid\
				<< " at ["\
				<< std::hex << index << "]" << std::endl;
			assert(commands.event_graph);
            Node *node = commands.event_graph->id_to_node(gid);
            if (node == nullptr) {
				std::cout << "Error: no gid found" << std::endl;
				return;
            }
			node->show_event_detail(index, std::cout);
		}, "Rendering even information in (gid, index_to_event)");

	view->add("converge", [&](Arguments args) {
			args.shouldHave(3);
			uint64_t gid_1 = args.get_num(0, "hex");
			uint64_t gid_2 = args.get_num(1, "hex");
			uint64_t max_len = args.get_num(2, "hex");

			if (!commands.event_graph)
				return;
			if (!commands.search_engine)
            	commands.search_engine = new GraphSearcher(commands.event_graph);

			uint64_t ret = commands.search_engine->check_converge(gid_1, gid_2, max_len);

			std::cout << "0x" << std::hex << gid_1 << " and " << std::hex << gid_2;
			if (ret > 0)
				std::cout << " converts to " << std::hex << ret << std::endl;
			else
				std::cout << " not converged" << std::endl;
				
		}, "check if a pair of nodes converges to a parent in event graph with thresh_hold times");

	view->add("path", [&](Arguments args) {
			args.shouldHave(3);
			uint64_t gid_from = args.get_num(0, "hex");
			uint64_t gid_to = args.get_num(1, "hex");
			std::string output_path = args.get(2);
			CriticalPath(commands.event_graph, gid_from, gid_to, output_path);
	    }, "check path from arg1 to arg2 in event graph");

	view->add("procs-hierachy", [&](Arguments args) {
			if (commands.event_graph == nullptr) return;
			args.shouldHave(1);
			commands.event_graph->direct_communicate_procs(args.get(0), std::cout);
		}, "Show the process communication hierachy with proc name as input");

}

void ArgusLoop::register_tfl_commands()
{
	ArgusLoop &commands = *this;
	CompositeCommand *tfl = new CompositeCommand("tfl", "traces for machine learning");
	this->add(tfl);
	/*
    tfl->add("import-graph", [&](Arguments args) {
            args.shouldHave(1); 
            if (commands.import_graph) {
                delete commands.import_graph;
                commands.import_graph = nullptr;
            }
            commands.import_graph = new ImportedGraph(commands.event_graph, args.get(0));
        }, "import graph edges from edge file");
	*/

    tfl->add("export-graph", [&](Arguments args) {
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
	/*
	tfl->add("tfl-export-edges", [&](Arguments args) {
			args.shouldHave(4);
			uint64_t from, to;
			args.asDec(0, &from);
			args.asDec(1, &to);
			std::string weak_edge_path = args.get(2);
			std::string output_path = args.get(3);
			//tfl_graph->tfl_edges_between(from, to, weak_edge_path, output_path);
		}, "export edgs between two events' tfl indexes");
	*/

	tfl->add("index2event", [&](Arguments args) {
			args.shouldHave(1);
			uint64_t index = args.get_num(0, "dec");
			commands.event_graph->show_event_info(index, std::cout);
		}, "show event information for the index of event");

	/*
	tfl->add("export-by-thread", [&](Arguments args) {
			args.shouldHave(1);
			commands.event_graph->tfl_by_thread(args.get(0));
		}, "flush the event to file");
	*/
	
	tfl->add("export-sequeces-pairs", [&](Arguments args) {
			args.shouldHave(1);
            if (!commands.search_engine)
            	commands.search_engine = new GraphSearcher(commands.event_graph);
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			commands.tfl_graph->tfl_edge_sequences(args.get(0));
		}, "export sequence pairs (positive and negtive) to file");

	tfl->add("collect-corner-edges", [&](Arguments args) {
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			int ret = (commands.tfl_graph->collect_inspect_edges()).size();
			LOG_S(INFO) << "Number of suspect edges = " << std::dec << ret << std::endl;
		}, "numer of suspect edges");

	tfl->add("show-edge", [&](Arguments args) {
			args.shouldHave(1);
			uint64_t index = args.get_num(0, "dec");
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			commands.tfl_graph->examine_edge(index);
		}, "the i_th edge for inspection");

	tfl->add("validate-edge", [&](Arguments args) {
			args.shouldHave(2);
			uint64_t index = args.get_num(0, "hex");
			uint64_t val = args.get_num(1, "hex");
			if (commands.tfl_graph == nullptr)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);
			commands.tfl_graph->validate_edge(index, val == 0? false: true);
		}, "validate edge with index and truth 0/1");
	/*
	tfl->add("validate-weak-edges", [&](Arguments args) {
			args.shouldHave(0);
			if (!commands.event_graph)
				return;
			WeakEdgeCalculator(commands.event_graph);
		}, "validate weak edges with event pairs instrumented in app");
	*/
	tfl->add("save-weak-edges", [&](Arguments args) {
			args.shouldHave(1);
			std::string path_prefix = args.get(0);
			
			if (!commands.event_graph)
				return;

			if (!commands.search_engine)
            	commands.search_engine = new GraphSearcher(commands.event_graph);

			if (!commands.tfl_graph)
				commands.tfl_graph = new TraceForLearn(commands.event_graph, commands.search_engine);

			WeakEdgeCalculator weak_edges(commands.event_graph);
			commands.tfl_graph->tfl_edges(weak_edges.get_positive_weaks(), path_prefix + ".positive");
			commands.tfl_graph->tfl_edges(weak_edges.get_negative_weaks(), path_prefix + ".negtive");
		}, "save weak-edges negtive/positive pairs with instrumentation in app");
}

void ArgusLoop::register_path_commands()
{

}
#if 0
{
	ArgusLoop &commands = *this;
	
	this->add("extract-main-path", [&](Arguments args) {
			args.shouldHave(4);
			uint64_t gid_from = args.get_num(0, "hex");
			uint64_t index_from = args.get_num(1, "hex");
			uint64_t gid_to = args.get_num(2, "hex");
			uint64_t index_to = args.get_num(3, "hex");
			std::cout << std::hex << "0x" << gid_from << "\t0x" << index_from << std::endl;
			std::cout << std::hex << "0x" << gid_to << "\t0x" << index_to << std::endl;

			EventBase *begin = commands.event_graph->get_event(gid_from, index_from);
			EventBase *end = commands.event_graph->get_event(gid_to, index_to);
			assert(begin && end);

			FillPath fillpath(commands.event_graph, begin, end);
			std::string path("./output/main_path");
			path += std::to_string(gid_from) + "_" + std::to_string(gid_to) + ".log";
			fillpath.save_path_to_file(path); 
	}, "check path from gid_arg1, index_arg2 to gid_arg3, index_arg4 in event graph");

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

	this->add("check-groups", [&](Arguments args) {
			args.shouldHave(0);
			VerticesClass checker(commands.event_graph);
			checker.render_statistics();
		}, "check groups categories");
}
#endif

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

#if defined(__APPLE__)
void ArgusLoop:: invoke_lldb_script()
{
	// Initialize the Python interpreter.
	Py_Initialize();
	//TODO: import python debugging script
	// Destroy the Python interpreter.
	Py_Finalize();
}
#endif

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
	commands.register_tfl_commands();
	commands.register_interactive_debugger_commands();


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
				LOG_S(INFO) << "Execute: " <<  line << std::endl;
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

