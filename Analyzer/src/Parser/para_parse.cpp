#include "parser.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>
typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
//typedef std::list<EventBase *> event_list_t;

void Parse::extra_symbolization(std::map<uint64_t, Parser *> &parsers)
{
    std::cerr << "Symbolize additional events with backtrace parser ... " << std::endl;
    if (parsers.find(BACKTRACE) == parsers.end()) {
        std::cerr << "No backtrace infomation found for " << __func__ << std::endl;
        return;
    }

    BacktraceParser *backtraceparser =
        dynamic_cast<BacktraceParser*>(parsers[BACKTRACE]);
    if (!backtraceparser) {
        std::cerr << "No backtrace parser found for " << __func__ << std::endl;
        return;
    }

    if (parsers.find(INTR) != parsers.end()) {
        IntrParser *intrparser =
            dynamic_cast<IntrParser*>(parsers[INTR]);
		//get_all proc
        std::map<std::pair<pid_t, std::string>, event_list_t> &proc_event_list_map
			= intrparser->get_event_list_map();
		std::cerr << "size of proc map = " << proc_event_list_map.size() << std::endl;
		auto it = proc_event_list_map.begin();
		for (; it != proc_event_list_map.end(); it++) {
			//if ((it->first).second == LoadData::meta_data.host)
        		intrparser->symbolize_intr_for_proc(backtraceparser, it->first);
		}
    }

    if (parsers.find(DISP_EXE) != parsers.end()) {
        DispatchParser *dispparser =
            dynamic_cast<DispatchParser *>(parsers[DISP_EXE]);
		//get_all proc
        std::map<std::pair<pid_t, std::string>, event_list_t> &proc_event_list_map
			= dispparser->get_event_list_map();
	
		auto it = proc_event_list_map.begin();
		for (; it != proc_event_list_map.end(); it++)
			//if ((it->first).second == LoadData::meta_data.host)
        		dispparser->symbolize_block_for_proc(backtraceparser, it->first);
    }

    if (parsers.find(DISP_DEQ) != parsers.end()) {
        DispatchParser *dispparser =
            dynamic_cast<DispatchParser *>(parsers[DISP_DEQ]);

		//get_all proc
        std::map<std::pair<pid_t, std::string>, event_list_t> &proc_event_list_map
			= dispparser->get_event_list_map();
		auto it = proc_event_list_map.begin();
		for (; it != proc_event_list_map.end(); it++)
			//if ((it->first).second == LoadData::meta_data.host)
        		dispparser->symbolize_block_for_proc(backtraceparser, it->first);
    }

    if (parsers.find(RL_BOUNDARY) != parsers.end()) {
        RLBoundaryParser *rlboundaryparser =
            dynamic_cast<RLBoundaryParser *>(parsers[RL_BOUNDARY]);

		//get_all proc
        std::map<std::pair<pid_t, std::string>, event_list_t> &proc_event_list_map
			= rlboundaryparser->get_event_list_map();
		auto it = proc_event_list_map.begin();
		for (; it != proc_event_list_map.end(); it++)
			//if ((it->first).second == LoadData::meta_data.host)
        		rlboundaryparser->symbolize_rlblock_for_proc(backtraceparser, it->first);
    }

    if (parsers.find(BREAKPOINT_TRAP) != parsers.end()) {
        BreakpointTrapParser *hwbrparser =
            dynamic_cast<BreakpointTrapParser *>(parsers[BREAKPOINT_TRAP]);
        hwbrparser->symbolize_hwbrtrap(backtraceparser);
    }

    if (parsers.find(MACH_CALLCREATE) != parsers.end()) {
        TimercallParser *timerparser = 
            dynamic_cast<TimercallParser *>(parsers[MACH_CALLCREATE]);

		//get_all proc
        std::map<std::pair<pid_t, std::string>, event_list_t> &proc_event_list_map
			= timerparser->get_event_list_map();
		auto it = proc_event_list_map.begin();
		for (; it != proc_event_list_map.end(); it++)
			//if ((it->first).second == LoadData::meta_data.host)
        		timerparser->symbolize_func_ptr_for_proc(backtraceparser, it->first);
    }
    std::cerr << "Finished parsing!\n";
}

std::map<uint64_t, std::list<EventBase *> > Parse::parse(std::map<uint64_t, std::string>& files)
{
    std::map<uint64_t, std::string>::iterator it;
    std::map<uint64_t, Parser *> parsers;
    std::map<uint64_t, Parser *>::iterator th_it;
    Parser *parser;
    std::map<uint64_t, std::list<EventBase *>> ret_lists;
    std::list<EventBase *> result;

    boost::asio::io_service ioService;
    boost::thread_group threadpool;
    asio_worker work(new asio_worker::element_type(ioService));

    for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
        threadpool.create_thread( boost::bind(&boost::asio::io_service::run,
                    &ioService));

    for (it = files.begin(); it != files.end(); it++) {
        parser = nullptr;
        switch(it->first) {
            case MACH_IPC_MSG: 
                parser = new MachmsgParser(it->second);
                break;
            case MACH_IPC_VOUCHER_INFO: 
            case MACH_IPC_VOUCHER_CONN:
            case MACH_IPC_VOUCHER_TRANSIT:
            case MACH_IPC_VOUCHER_DEALLOC:
            case MACH_BANK_ACCOUNT:
                parser = new VoucherParser(it->second);
                break;
            case MACH_MK_RUN:
                parser = new MkrunParser(it->second);
                break;
            case INTR:
                parser = new IntrParser(it->second);
                break;
            case MACH_TS:
                parser = new TsmaintainParser(it->second);
                break;
            case MACH_WAIT:
                parser = new WaitParser(it->second);
                break;
            case DISP_ENQ:
            case DISP_DEQ:
            case DISP_EXE:
            case DISP_MIG:
                parser = new DispatchParser(it->second);
                break;
            case MACH_CALLCREATE:
            case MACH_CALLOUT:
            case MACH_CALLCANCEL:
                parser = new TimercallParser(it->second);
                break;
            case BACKTRACE:
                parser = new BacktraceParser(it->second,
                        LoadData::meta_data.libinfo_file);
                break;
            case MACH_SYS:
            case BSD_SYS:
                parser = new SyscallParser(it->second);
                break;
            case CA_SET:
            case CA_DISPLAY:
                parser = new CAParser(it->second);
                break;
            case BREAKPOINT_TRAP:
                parser = new BreakpointTrapParser(it->second);
                break;
            case RL_OBSERVER:
                parser = new RLObserverParser(it->second);
                break;
            case EVENTREF:
                parser = new EventRefParser(it->second);
                break;
            case NSAPPEVENT:
                parser = new NSAppEventParser(it->second);
                break;
            case RL_BOUNDARY:
                parser = new RLBoundaryParser(it->second);
                break;
            case APP_LOG:
                parser = new AppLogParser(it->second);
                break;

            default:
                break;
        }        

        if (parser) {
            parsers[it->first] = parser;
            //ioService.post(boost::bind([parser] {parser->process();}));
            ioService.post(boost::bind(&Parser::process, parser));
        } else {
            std::cerr << "Check : fail to begin parser " << it->first << std::endl;
        }
    }
    work.reset();
    threadpool.join_all();
    //ioService.stop();

    extra_symbolization(parsers);

    for (th_it = parsers.begin(); th_it != parsers.end(); th_it++) {
        result.insert(result.end(), (th_it->second)->collect().begin(),
                (th_it->second)->collect().end());
        ret_lists[th_it->first] = (th_it->second)->collect();
        delete th_it->second;
    }
    parsers.clear();

    assert(ret_lists.find(0) == ret_lists.end());
    ret_lists[0] = result;
    return ret_lists;
}

std::string Parse::get_prefix(std::string &input_path)
{
    std::string filename;
    size_t dir_pos = input_path.find_last_of("/");
    if (dir_pos != std::string::npos)
        filename = input_path.substr(dir_pos + 1);
    else
        filename = input_path;

    boost::filesystem::path dir("temp");
    if (!(boost::filesystem::exists(dir)))  
        boost::filesystem::create_directory(dir);

    if ((boost::filesystem::exists(dir))) {
        std::string prefix("temp/");
        filename = prefix + filename;
    }

    size_t pos = filename.find(".");
    if (pos != std::string::npos)
        return filename.substr(0, pos);
    else
        return filename;
}

std::map<uint64_t, std::string> Parse::divide_files(std::string filename,
        uint64_t (*hash_func)(uint64_t, std::string))
{
    std::map<uint64_t, std::string> div_files;
    std::ifstream infile(filename);

    if (infile.fail()) {
        std::cerr << "Error: faild to open file: " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    std::map<uint64_t, std::ofstream*> filep;
    std::string line, abstime, deltatime, opname;
    uint64_t arg1, arg2, arg3, arg4, tid, hashval; 

    while (getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> abstime >> deltatime >> opname >> std::hex >> arg1 \
                    >> arg2 >> arg3 >> arg4 >> tid)) {
            break;
        }

        hashval = hash_func(tid, opname);
        // 0 is reserved for irrelated tracing points
        if (hashval == 0) {
            continue;
        }

        if (filep.find(hashval) == filep.end()) {
            filep[hashval] = new std::ofstream(get_prefix(filename)
                    + "." + std::to_string(hashval) + ".tmp");

            if (!filep[hashval] || !*filep[hashval]) {
                std::cerr << "Error: unable to create std::ofstream for ";
                if (filep[hashval])
                    std::cerr << "file ";
                std::cerr << hashval << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        *(filep[hashval]) << line.c_str() << std::endl;        
    }
    infile.close();

    std::map<uint64_t, std::ofstream*>::iterator it;
    for (it = filep.begin(); it != filep.end(); ++it) {
        div_files[it->first] = get_prefix(filename)
            + "." + std::to_string(it->first) + ".tmp";
        (it->second)->close();
        delete it->second;
    }
    return div_files;
}

std::map<uint64_t, std::list<EventBase *> > Parse::divide_and_parse()
{
    std::map<uint64_t, std::string> files = divide_files(LoadData::meta_data.data,
            LoadData::map_op_code);
    std::map<uint64_t, std::list<EventBase*>> lists = parse(files);
    return lists;
}

std::list<EventBase *> Parse::parse_backtrace()
{
    std::map<uint64_t, std::string> files = divide_files(LoadData::meta_data.data, LoadData::map_op_code);
    BacktraceParser parser(files[BACKTRACE], LoadData::meta_data.libinfo_file);
    parser.process();
    return parser.collect();
}
