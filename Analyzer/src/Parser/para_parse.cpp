#include "parser.hpp"
#include "eventlistop.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>

using namespace std;
typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;
namespace Parse
{
	static void extra_symbolization(map<uint64_t, Parser *> &parsers)
	{
		cerr << "Symbolize additional events with backtrace parser ... \n";
		if (parsers.find(BACKTRACE) != parsers.end()) {
			BacktraceParser *backtraceparser =
				dynamic_cast<BacktraceParser*>(parsers[BACKTRACE]);
			if (backtraceparser) {
				if (parsers.find(INTR) != parsers.end()) {
					IntrParser *intrparser =
						dynamic_cast<IntrParser*>(parsers[INTR]);
					intrparser->symbolize_intr_rip(backtraceparser);
				}

				if (parsers.find(DISP_EXE) != parsers.end()) {
					DispatchParser *dispparser =
						dynamic_cast<DispatchParser *>(parsers[DISP_EXE]);
					dispparser->symbolize_dispatch_event(backtraceparser);
				}

				if (parsers.find(DISP_DEQ) != parsers.end()) {
					DispatchParser *dispparser =
						dynamic_cast<DispatchParser *>(parsers[DISP_DEQ]);
					dispparser->symbolize_dispatch_event(backtraceparser);
				}
			
				if (parsers.find(BREAKPOINT_TRAP) != parsers.end()) {
					BreakpointTrapParser *hwbrparser =
						dynamic_cast<BreakpointTrapParser *>(parsers[BREAKPOINT_TRAP]);
					hwbrparser->symbolize_hwbrtrap(backtraceparser);
				}
			}
		}
		cerr << "Finished parsing!\n";
	}

	map<uint64_t, list<event_t *>> parse(map<uint64_t, string>& files)
	{
		map<uint64_t, string>::iterator it;
		map<uint64_t, Parser *> parsers;
		map<uint64_t, Parser *>::iterator th_it;
		Parser *parser;
		map<uint64_t, list<event_t *>> ret_lists;
		list<event_t *> result;

		boost::asio::io_service ioService;
		boost::thread_group threadpool;
		asio_worker work(new asio_worker::element_type(ioService));

		for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
			threadpool.create_thread( boost::bind(&boost::asio::io_service::run,
				&ioService));

		for (it = files.begin(); it != files.end(); it++) {
			parser = NULL;
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
				case WQ_NEXT:
					parser = new WqnextParser(it->second);
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
				default:
					break;
			}		

			if (parser) {
				parsers[it->first] = parser;
				//ioService.post(boost::bind([parser] {parser->process();}));
				ioService.post(boost::bind(&Parser::process, parser));
			} else {
				cerr << "Check : fail to begin parser " << it->first << endl;
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

		EventListOp::sort_event_list(result);
		assert(ret_lists.find(0) == ret_lists.end());
		ret_lists[0] = result;
		return ret_lists;
	}
	
	static string get_prefix(string &input_path)
	{
		string filename;
		size_t dir_pos = input_path.find_last_of("/");
		if (dir_pos != string::npos)
			filename = input_path.substr(dir_pos + 1);
		else
			filename = input_path;

		boost::filesystem::path dir("temp");
		if (!(boost::filesystem::exists(dir)))  
			boost::filesystem::create_directory(dir);

		if ((boost::filesystem::exists(dir))) {
			string prefix("temp/");
			filename = prefix + filename;
		}
		
		size_t pos = filename.find(".");
		if (pos != string::npos)
			return filename.substr(0, pos);
		else
			return filename;
	}

	map<uint64_t, string> divide_files(string filename,
		uint64_t (*hash_func)(uint64_t, string))
	{
		map<uint64_t, string> div_files;
		ifstream infile(filename);

		if (infile.fail()) {
			cerr << "Error: faild to open file: " << filename << endl;
			exit(EXIT_FAILURE);
		}

		map<uint64_t, ofstream*> filep;
		string line, abstime, deltatime, opname;
		uint64_t arg1, arg2, arg3, arg4, tid, hashval; 
		
		while (getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> abstime >> deltatime >> opname >> hex >> arg1 \
				>> arg2 >> arg3 >> arg4 >> tid))
				break;

			hashval = hash_func(tid, opname);
			// 0 is reserved for irrelated tracing points
			if (hashval == 0) 
				continue;

			if (filep.find(hashval) == filep.end()) {
				filep[hashval] = new ofstream(get_prefix(filename)
					+ "." + to_string(hashval) + ".tmp");

				if (!filep[hashval] || !*filep[hashval]) {
					cerr << "Error: unable to create ofstream for ";
					if (filep[hashval])
						cerr << "file ";
					cerr << hashval << endl;
					exit(EXIT_FAILURE);
				}
			}

			*(filep[hashval]) << line.c_str() << endl;		
		}
		infile.close();
	
		map<uint64_t, ofstream*>::iterator it;
		for (it = filep.begin(); it != filep.end(); ++it) {
			div_files[it->first] = get_prefix(filename)
				+ "." + to_string(it->first) + ".tmp";
			(it->second)->close();
			delete it->second;
		}
		return div_files;
	}

	map<uint64_t, list<event_t *>> divide_and_parse()
	{
		map<uint64_t, string> files = divide_files(LoadData::meta_data.datafile,
			LoadData::map_op_code);
		map<uint64_t, list<event_t*>> lists = parse(files);
		return lists;
	}

	list<event_t *> parse_backtrace()
	{
		map<uint64_t, string> files = divide_files(LoadData::meta_data.datafile, LoadData::map_op_code);
		BacktraceParser parser(files[BACKTRACE], LoadData::meta_data.libinfo_file);
		parser.process();
		return parser.collect();
	}
}
