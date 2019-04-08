#ifndef PARSER_HPP
#define PARSER_HPP

#include "base.hpp"
#include "backtraceinfo.hpp"
#include "parse_helper.hpp"
#include "loader.hpp"
#include <stdio.h>

namespace Parse
{
	class Parser {
		string filename;
	protected:
		ifstream infile;
		ofstream outfile;
		list<event_t *> local_event_list;
	public:
		Parser(string _filename) {
			filename = _filename;
			infile.open(filename);
			if (infile.fail()) {
				cerr << "Error: faile to open file for reading " << filename << endl;
				exit(EXIT_FAILURE);
			}
			outfile.open(filename+".remain");
			if (outfile.fail()) {
				cerr << "Error: fail to open file for writing " << filename + ".remain" << endl;
				infile.close();
				exit(EXIT_FAILURE);
			}
		}
		virtual ~Parser() {
			long pos = outfile.tellp();
			if (infile.is_open())
				infile.close();
			if (outfile.is_open())
				outfile.close();
			local_event_list.clear();
			if (pos == 0) {
				remove(filename.c_str());
				remove((filename + ".remain").c_str());
			} else {
				cerr << "Warning : file " << filename << " is not parsed completely" << endl;
			}
		}
		list<event_t *> &collect(void) {return local_event_list;}
		virtual void process() {}
	};
	
	class MachmsgParser: public Parser {
		map<uint64_t, msg_ev_t *> msg_events;
		map<string, uint64_t> collector;
		bool process_kmsg_end(string opname, double abstime, istringstream &iss);
		bool process_kmsg_set_info(string opname, double abstime, istringstream &iss);
		bool process_kmsg_begin(string opname, double abstime, istringstream &iss);
		msg_ev_t *new_msg(string opname, uint64_t tag, double abstime, uint64_t tid, uint64_t coreid, string procname);
		msg_ev_t *check_mig_recv_by_send(msg_ev_t *cur_msg, uint64_t msgh_id);
		bool process_kmsg_free(string opname, double abstime, istringstream &iss);
	public:
		MachmsgParser(string filename);
		void process();
	};
	
	class VoucherParser: public Parser {
		bool process_voucher_info(string opname, double abstime, istringstream &iss);
		bool process_voucher_conn(string opname, double abstime, istringstream &iss);
		bool process_voucher_transit(string opname, double abstime, istringstream &iss);
		bool process_voucher_deallocate(string opname, double abstime, istringstream &iss);
		bool process_bank_account(string opname, double abstime, istringstream &iss);
	public:
		VoucherParser(string filename);
		void process();
	};

	class MkrunParser: public Parser {
		map<uint64_t, mkrun_ev_t *> mkrun_events;
		bool set_info(mkrun_ev_t *mr_event, uint64_t peer_prio, uint64_t wait_result, uint64_t run_info);
		bool process_finish_wakeup(double abstime, istringstream &iss);
		bool process_new_wakeup(double abstime, istringstream &iss);
	public:
		MkrunParser(string filename);
		void process();
	};

	class BacktraceParser;
	class IntrParser: public Parser {
		map<uint64_t, intr_ev_t *> intr_events;
	public:
		IntrParser(string filename);
		intr_ev_t *create_intr_event(double abstime, string opname, istringstream &iss);
		bool collect_intr_info(double abstime, istringstream &iss);
		void process();
		void symbolize_intr_rip(BacktraceParser *backtrace_parser);
	};

	class WqnextParser: public Parser {
		map<uint64_t, wqnext_ev_t*> wqnext_events;
		bool set_info(wqnext_ev_t *wqnext_event, uint64_t wqnext_type, uint64_t next_thread, uint64_t arg3);
		bool finish_wqnext_event(string opname, double abstime, istringstream &iss);
		bool new_wqnext_event(string opname, double abstime, istringstream &iss);
	public:
		WqnextParser(string filename);
		void process();
	};
	
	class TsmaintainParser: public Parser {
	public:
		TsmaintainParser(string filename);
		void process();
	};
	
	class WaitParser: public Parser {
		map<uint64_t, wait_ev_t *> wait_events;
		bool set_info(wait_ev_t *wait_event, uint64_t pid, uint64_t deadline, uint64_t wait_result);
		bool new_wait_event(string opname, double abstime, istringstream &iss);
		bool finish_wait_event(string opname, double abstime, istringstream &iss);
	public:
		WaitParser(string filename);
		void process();
	};
	
	class DispatchParser: public Parser {
		map<uint64_t, dequeue_ev_t*> dequeue_events;
		list<blockinvoke_ev_t *> dispatch_blockinvoke_begin_list;

		bool is_duplicate_deq(dequeue_ev_t *prev, dequeue_ev_t *cur);
		bool set_info_for_dequeue(double abstime, uint32_t ref, istringstream &iss);
		bool new_dequeue_event(uint32_t ref, string opname, double abstime, istringstream &iss);
		bool checking_blockinvoke_pair(blockinvoke_ev_t *new_invoke);
		bool process_enqueue(string opname, double abstime, istringstream &iss);
		bool process_dequeue(string opname, double abstime, istringstream &iss);
		bool process_blockinvoke(string opname, double abstime, istringstream &iss);
		bool process_migservice(string opname, double abstime, istringstream &iss);
	public:
		DispatchParser(string filename);
		void process();
		void symbolize_dispatch_event(BacktraceParser *parser);
		void symbolize_dequeue(debug_data_t *cur_debugger_ptr);
		void symbolize_blockinvoke(debug_data_t *cur_debugger_ptr);
	};

	class TimercallParser:public Parser {
		bool process_timercreate(string opname, double abstime, istringstream &iss);
		bool process_timercallout(string opname, double abstime, istringstream &iss);
		bool process_timercancel(string opname, double abstime, istringstream &iss);
	public:
		TimercallParser(string filename);
		void process();
	};
	
	class BacktraceParser: public Parser {
		#define TRACE_MAX 32
		map<uint64_t, images_t*> images_events;
		map<uint64_t, backtrace_ev_t*> backtrace_events;
		//map<string, images_t *> proc_images_map;
		//map<string, list<backtrace_ev_t*>>proc_backtraces_map;
		map<pair<pid_t, string>, images_t *> proc_images_map;
		map<pair<pid_t, string>, list<backtrace_ev_t *> >proc_backtraces_map;
		map<string, map<uint64_t, string> >image_vmsymbol_map;
		string path_log;

		typedef struct {
			uint64_t vm_offset;
			queue<uint64_t> path;
		} cur_path_t;
		map<uint64_t, cur_path_t*> image_subpaths;

		bool try_add_path_to_image(cur_path_t *cur_path, images_t *cur_image);
		bool collect_path_info(cur_path_t *cur_path, images_t *image, uint64_t vm_offset, uint64_t *addr, uint64_t size);
		bool collect_image_for_proc(images_t *cur_image);
		void collect_frames(backtrace_ev_t *backtrace_event, double abstime, uint64_t *frames, uint64_t size);
		void collect_backtrace_for_proc(backtrace_ev_t *backtrace_event, string procname);
		void process_path_from_log(void);
		bool process_path(string opname, double abstime, istringstream &iss);
		bool process_frame(string opname, double abstime, istringstream &iss);
		void symbolize_backtraces();
	public:
		BacktraceParser(string filename, string path_log);
		~BacktraceParser() {clear();}
		images_t *get_host_image();
		images_t *get_image_for_proc(pid_t pid, string procname);
		map<string, map<uint64_t, string> > &get_vmsymbol_map();
		bool setup_lldb(debug_data_t *debugger_data, images_t *images);
		void process();
		void clear();
	};
	
	class SyscallParser: public Parser {
		map<uint64_t, syscall_ev_t *> syscall_events;
		bool new_msc(syscall_ev_t *syscall_event, uint64_t tid, string opname, uint64_t *args, int size);
		bool process_msc(string opname, double abstime, bool is_begin, istringstream &iss);
		bool new_bsc(syscall_ev_t *syscall_event, uint64_t tid, string opname, uint64_t *args, int size);
		bool process_bsc(string opname, double abstime, bool is_begin, istringstream &iss);
	public:
		SyscallParser(string filename);
		void process();
	};
	
	class CAParser: public Parser {
		bool process_caset(string opname, double abstime, istringstream &iss);
		bool process_cadisplay(string opname, double abstime, istringstream &iss);
	public:
		CAParser(string filename);
		void process();
	};

	class BreakpointTrapParser: public Parser {
		map<uint64_t, breakpoint_trap_ev_t *> breakpoint_trap_events;
		void symbolize_addr(BacktraceParser *, images_t *, breakpoint_trap_ev_t * breakpoint_trap_event, debug_data_t *cur_debugger);
		void symbolize_eip(BacktraceParser *, images_t *, breakpoint_trap_ev_t * breakpoint_trap_event, debug_data_t *cur_debugger);
		void symbolize_hwbrtrap_for_proc(BacktraceParser *backtrace_parser, string procname);
	public:
		BreakpointTrapParser(string filename);
		void symbolize_hwbrtrap(BacktraceParser *backtrace_parser);
		void process();
	};
	
	class EventRefParser: public Parser {
	public:
		EventRefParser(string filename);
		void process();
	};
	
	class RLObserverParser: public Parser {
	public:
		RLObserverParser(string filename);
		void process();
	};

	class NSAppEventParser: public Parser {
		map<uint64_t, nsapp_event_ev_t *> nsappevent_events;
	public:
		NSAppEventParser(string filename);
		void process();
	};
	
	class RLBoundaryParser: public Parser {
		void symbolize_func_ptr(debug_data_t *cur_debugger_ptr);
	public:
		RLBoundaryParser(string filename);
		void symbolize_rlboundary_callbacks(BacktraceParser *parser);
		void process();
	};
}
#endif
