#ifndef PARSER_HPP
#define PARSER_HPP

#include "base.hpp"
#include "backtraceinfo.hpp"
#include "parse_helper.hpp"
#include "loader.hpp"
#include <stdio.h>

namespace Parse
{
    typedef std::list<EventBase *> event_list_t;
    class Parser {
    protected:
        //typedef std::list<EventBase *> event_list_t;
        std::string filename;
        std::ifstream infile;
        std::ofstream outfile;
        event_list_t local_event_list;
		
        //std::map<std::string, event_list_t> proc_event_list_map;
        //event_list_t get_events_for_proc(std::string);
        //void add_to_proc_map(std::string, EventBase *event);
        std::map<std::pair<pid_t, std::string>, event_list_t> proc_event_list_map;
        event_list_t get_events_for_proc(std::pair<pid_t, std::string>);
        void add_to_proc_map(std::pair<pid_t, std::string>, EventBase *event);

    public:
        Parser(std::string _filename);
        virtual ~Parser();
        event_list_t &collect(void) {return local_event_list;}
        std::map<std::pair<pid_t, std::string>, event_list_t> &get_event_list_map() {return proc_event_list_map;}
        virtual void process() {}
    };

    class BacktraceParser;
    
    class MachmsgParser: public Parser {
        std::map<uint64_t, MsgEvent *> msg_events;
        std::map<std::string, uint64_t> collector;

        bool process_kmsg_end(std::string opname, double abstime, std::istringstream &iss);
        bool process_kmsg_set_info(std::string opname, double abstime, std::istringstream &iss);
        bool process_kmsg_begin(std::string opname, double abstime, std::istringstream &iss);
        bool process_kmsg_free(std::string opname, double abstime, std::istringstream &iss);

        MsgEvent *new_msg(std::string opname, uint64_t tag, double abstime,
            uint64_t tid, uint64_t coreid, std::string procname);
        MsgEvent *check_mig_recv_by_send(MsgEvent *cur_msg, uint64_t msgh_id);

    public:
        MachmsgParser(std::string filename);
        void process();
    };
    
    class VoucherParser: public Parser {
        bool process_voucher_info(std::string opname, double abstime, std::istringstream &iss);
        bool process_voucher_conn(std::string opname, double abstime, std::istringstream &iss);
        bool process_voucher_transit(std::string opname, double abstime, std::istringstream &iss);
        bool process_voucher_deallocate(std::string opname, double abstime, std::istringstream &iss);
        bool process_bank_account(std::string opname, double abstime, std::istringstream &iss);
    public:
        VoucherParser(std::string filename);
        void process();
    };

    class MkrunParser: public Parser {
        std::map<uint64_t, MakeRunEvent *> mkrun_events;
        bool set_info(MakeRunEvent *mr_event, uint64_t peer_prio, uint64_t wait_result, uint64_t run_info);
        bool process_finish_wakeup(double abstime, std::istringstream &iss);
        bool process_new_wakeup(double abstime, std::istringstream &iss);
    public:
        MkrunParser(std::string filename);
        void process();
    };

    class IntrParser: public Parser {
        //std::map<std::string, event_list_t> proc_event_list_map;
    private:
        std::map<uint64_t, IntrEvent *> intr_events;
        IntrEvent *create_intr_event(double abstime, std::string opname, std::istringstream &iss);
        bool gather_intr_info(double abstime, std::istringstream &iss);
        //void add_to_proc_map(std::string, EventBase *event);
    public:
        IntrParser(std::string filename);
        void process();
        void symbolize_intr_for_proc(BacktraceParser *backtrace_parser, std::pair<pid_t, std::string> proc);
    };

    class WqnextParser: public Parser {
        std::map<uint64_t, WorkQueueNextItemEvent*> wqnext_events;
        bool set_info(WorkQueueNextItemEvent *wqnext_event, uint64_t wqnext_type, uint64_t next_thread, uint64_t arg3);
        bool finish_wqnext_event(std::string opname, double abstime, std::istringstream &iss);
        bool new_wqnext_event(std::string opname, double abstime, std::istringstream &iss);
    public:
        WqnextParser(std::string filename);
        void process();
    };
    
    class TsmaintainParser: public Parser {
    public:
        TsmaintainParser(std::string filename);
        void process();
    };
    
    class WaitParser: public Parser {
        std::map<uint64_t, WaitEvent *> wait_events;
        bool set_info(WaitEvent *wait_event, uint64_t pid, uint64_t deadline, uint64_t wait_result);
        bool new_wait_event(std::string opname, double abstime, std::istringstream &iss);
        bool finish_wait_event(std::string opname, double abstime, std::istringstream &iss);
    public:
        WaitParser(std::string filename);
        void process();
    };
    
    class DispatchParser: public Parser {
    public:
        typedef std::list<BlockInvokeEvent *> dispatch_block_invoke_list_t;              
        typedef std::list<BlockDequeueEvent *> dispatch_block_dequeue_list_t;
        typedef std::list<BlockEnqueueEvent *> dispatch_block_enqueue_list_t;

    private:
        std::map<uint64_t, BlockDequeueEvent*> dequeue_events;
        dispatch_block_invoke_list_t dispatch_block_invoke_begin_list;
        //std::map<std::string, event_list_t> proc_event_list_map;

        bool is_duplicate_deq(BlockDequeueEvent *prev, BlockDequeueEvent *cur);
        bool gather_dequeue_info(double abstime, uint32_t ref, std::istringstream &iss);
        bool create_dequeue_event(uint32_t ref, std::string opname, double abstime, std::istringstream &iss);
        bool checking_block_invoke_pair(BlockInvokeEvent *new_invoke);

        //void add_to_proc_map(std::string, EventBase *event);
        bool process_enqueue(std::string opname, double abstime, std::istringstream &iss);
        bool process_dequeue(std::string opname, double abstime, std::istringstream &iss);
        bool process_block_invoke(std::string opname, double abstime, std::istringstream &iss);
        bool process_migservice(std::string opname, double abstime, std::istringstream &iss);
        void symbolize_block_invoke(debug_data_t *, event_list_t);
        void symbolize_block_dequeue(debug_data_t *, event_list_t);
    public:
        DispatchParser(std::string filename);
        void process();
        void symbolize_block_for_proc(BacktraceParser *parser, std::pair<pid_t, std::string> proc);
    };

    class TimercallParser:public Parser {
    private:
        //std::map<std::string, event_list_t> proc_event_list_map;
        bool process_timercreate(std::string opname, double abstime, std::istringstream &iss);
        bool process_timercallout(std::string opname, double abstime, std::istringstream &iss);
        bool process_timercancel(std::string opname, double abstime, std::istringstream &iss);
        //void add_to_proc_map(std::string, EventBase *event);
    public:
        TimercallParser(std::string filename);
        void process();
        void symbolize_func_ptr_for_proc(BacktraceParser *parser, std::pair<pid_t, std::string> proc);
    };
    
    class BacktraceParser: public Parser {
    public:
        #define TRACE_MAX 32
        typedef struct {
            uint64_t vm_offset;
            std::queue<uint64_t> path;
            void init() {
                vm_offset = -1;
                std::queue<uint64_t> empty;
                std::swap(path, empty);
            }
        } raw_path_t;
        typedef std::list<BacktraceEvent *> backtrace_list_t;
        typedef std::pair<pid_t, std::string> proc_key_t;
        typedef std::map<addr_t, std::string> symmap_t;
        typedef std::map<std::string, symmap_t> path_to_symmap_t;

    private:
        std::string host_arch;
        std::string path_log;
        std::map<tid_t, BacktraceEvent*> backtrace_events;
        std::map<uint64_t, images_t*> images_events;
        std::map<uint64_t, raw_path_t*> image_subpaths;
        std::map<proc_key_t, images_t *> proc_images_map;
        std::map<proc_key_t, backtrace_list_t> proc_backtraces_map;
        path_to_symmap_t path_to_vmsym_map;

        //collect backtrace events
        void collect_backtrace_for_proc(BacktraceEvent *backtrace_event, std::string procname);
        void gather_frames(BacktraceEvent *backtrace_event, double abstime, uint64_t *frames, uint64_t size, bool is_end);
        bool process_frame(std::string opname, double abstime, std::istringstream &iss, bool is_end);
		std::list<BacktraceEvent *> get_event_list(std::pair<pid_t, std::string> key);


        //collect image information
        bool collect_image_for_proc(images_t *cur_image);
        bool complete_path(tid_t tid);
        bool gather_path(tid_t tid, addr_t vm_offset, addr_t *addrs);
        bool create_image(tid_t tid, std::string procname);
        bool process_path(std::string opname, double abstime, std::istringstream &iss);
        bool process_path_from_log(void);
        void symbolize_backtraces();
        
    public:
        BacktraceParser(std::string filename, std::string path_log);
        ~BacktraceParser();

        images_t *get_host_image();
        images_t *proc_to_image(pid_t pid, std::string procname);
        path_to_symmap_t &get_symbol_map() {return path_to_vmsym_map;}
        bool setup_lldb(debug_data_t *debugger_data, images_t *images);
        void process();
    };
    
    class SyscallParser: public Parser {
        std::map<uint64_t, SyscallEvent *> syscall_events;
        bool new_msc(SyscallEvent *syscall_event, uint64_t tid, std::string opname, uint64_t *args, int size);
        bool process_msc(std::string opname, double abstime, bool is_begin, std::istringstream &iss);
        bool new_bsc(SyscallEvent *syscall_event, uint64_t tid, std::string opname, uint64_t *args, int size);
        bool process_bsc(std::string opname, double abstime, bool is_begin, std::istringstream &iss);
    public:
        SyscallParser(std::string filename);
        void process();
    };
    
    class CAParser: public Parser {
        bool process_caset(std::string opname, double abstime, std::istringstream &iss);
        bool process_cadisplay(std::string opname, double abstime, std::istringstream &iss);
    public:
        CAParser(std::string filename);
        void process();
    };

    class BreakpointTrapParser: public Parser {
    //public:
       // typedef std::list<EventBase *> event_list_t;
    private:
        std::map<uint64_t, BreakpointTrapEvent *> breakpoint_trap_events;
        //std::map<std::string, event_list_t> proc_event_list_map;
        //void add_to_proc_map(std::string, EventBase *event);
        void symbolize_hwbrtrap_for_proc(BacktraceParser *, std::pair<pid_t, std::string> procname);

    public:
        BreakpointTrapParser(std::string filename);
        void process();
        void symbolize_hwbrtrap(BacktraceParser *backtrace_parser);
    };
    
    class EventRefParser: public Parser {
    public:
        EventRefParser(std::string filename);
        void process();
    };
    
    class NSAppEventParser: public Parser {
        std::map<uint64_t, NSAppEventEvent *> nsappevent_events;
    public:
        NSAppEventParser(std::string filename);
        void process();
    };
    
    class RLObserverParser: public Parser {
    public:
        RLObserverParser(std::string filename);
        void process();
    };

    class RLBoundaryParser: public Parser {
        typedef std::list<RunLoopBoundaryEvent *> rl_event_list_t;
        //std::map<std::string, event_list_t > proc_event_list_map;
        //void add_to_proc_map(std::string, EventBase *event);
    public:
        RLBoundaryParser(std::string filename);
        void symbolize_rlblock_for_proc(BacktraceParser *parser, std::pair<pid_t, std::string> proc);
        void process();
    };
    
    class AppLogParser: public Parser {
    public:
        AppLogParser(std::string filename);
        void process();
    };

    void extra_symbolization(std::map<uint64_t, Parser *> &parsers);
}
#endif
