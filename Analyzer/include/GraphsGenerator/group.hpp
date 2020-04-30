#ifndef GROUP_HPP
#define GROUP_HPP
#include <algorithm>
#include "base.hpp"
#include "mach_msg.hpp"
#include "mkrunnable.hpp"
#include "interrupt.hpp"
#include "workq_next.hpp"
#include "tsmaintenance.hpp"
#include "wait.hpp"
#include "syscall.hpp"
#include "dispatch.hpp"
#include "timer_call.hpp"
#include "voucher.hpp"
#include "backtraceinfo.hpp"
#include "rlobserver.hpp"
#include "breakpoint_trap.hpp"
#include "cadisplay.hpp"
#include "caset.hpp"
#include "rlboundary.hpp"

#include "eventlistop.hpp"
#include "parse_helper.hpp"

#include "msg_pattern.hpp"
#include "dispatch_pattern.hpp"
#include "timercall_pattern.hpp"
#include "mkrun_wait_pair.hpp"
#include "voucher_bank_attrs.hpp"
#include "core_animation_connection.hpp"
#include "nsappevent.hpp"
#include "caset.hpp"
#include "breakpoint_trap_connection.hpp"
#include "runloop_connection.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define GROUP_ID_BITS 20
#define DivideNewGroup 1
#define DivideOldGroup 2

class Group;
class Groups;

class GroupDivideFlag {
private:
    int divide_by_msg;
    int divide_by_wait;
    int divide_by_disp;
    int disp_divide_by_wait;
    uint64_t blockinvoke_level;
public:
    GroupDivideFlag();
    void set_divide_by_msg(int i) {divide_by_msg |= i;}
    int is_divide_by_msg(void) {return divide_by_msg;}
    void set_disp_divide_by_wait(int i) {disp_divide_by_wait |= i;}
    int is_disp_divide_by_wait(void) {return disp_divide_by_wait;}
    void set_divide_by_wait(int i) {divide_by_wait |= i;}
    int is_divide_by_wait(void) {return divide_by_wait;}
    void set_divide_by_disp(int i) {divide_by_disp |= i;}
    int is_divide_by_disp(void) {return divide_by_disp;}

    void set_blockinvoke_level(uint64_t level) {blockinvoke_level = level;}
    uint64_t get_blockinvoke_level(void) {return blockinvoke_level;}
    void blockinvoke_level_inc(void) {blockinvoke_level++;}
    void blockinvoke_level_dec(void) {blockinvoke_level--;}
};

class GroupSemantics {
    EventBase *root;
    EventBase *nsapp_event;
    EventBase *caset_event;
    std::set<std::string> group_peer;

    std::map<std::string, uint32_t> group_tags;

public:
	std::pair<int, std::list<uint64_t> > propagate_frames;

    GroupSemantics(EventBase *root);
    ~GroupSemantics();
    void set_root(EventBase *root);
    void add_group_peer(std::set<std::string>);
    void add_group_peer(std::string);


    void add_group_tags(std::string desc);
    std::map<std::string, uint32_t> &get_group_tags(void) {return group_tags;}

    EventBase *get_root(void) const {return root;}
    std::set<std::string> &get_group_peer(void) {return group_peer;}
    void set_nsappevent(EventBase *event) {nsapp_event = event;}
    void set_view_update(EventBase *event) {caset_event = event;}
    EventBase *contains_nsappevent(void) {return nsapp_event;}
    EventBase *contains_view_update(void) {return caset_event;}
};

class Group : public GroupDivideFlag, public GroupSemantics {
public:
    typedef uint64_t tid_t;
    typedef uint64_t group_id_t;
    typedef std::list<EventBase *> event_list_t;

private:
    bool sorted;
    double time_span;
    group_id_t group_id;
    event_list_t container;
    
    void set_group_id(group_id_t group_id);
    friend class Groups;

public:
    Group(uint64_t _group_id, EventBase *_root);
    Group(Group const &g):GroupDivideFlag(), GroupSemantics(g.get_root()){
		*this = g; 
		propagate_frames.first = -1;
	}
    /*TODO:construct groups from files*/
    /*Group(std::string file)*/
    ~Group();

    tid_t get_tid(void) {return get_first_event()->get_tid();}
    group_id_t get_group_id(void) {return group_id;}
    std::string get_procname(void);
	pid_t get_pid(void) {return get_first_event()->get_pid();}

	void propagate_bt(int index, BacktraceEvent *bt_event);
    void add_to_container(EventBase *);
    void add_to_container(Group *);
    void empty_container(void);
    void sort_container(void);
    std::list<EventBase *> &get_container(void) {return container;}
    uint64_t get_size(void) {return container.size();}
    double calculate_time_span(void);
    EventBase *get_last_event(void); 
    EventBase *get_first_event(void);
	EventBase *get_real_first_event(void);
	EventBase *event_at(uint64_t index);
    bool find_event(EventBase *event) {return event->get_group_id() == group_id ? true: false;}
    bool wait_over();

    /*  0; if equal
     *  1; if this > peer if applicable
     * -1: if this < peer if applicable
     */
    bool operator==(Group &peer);
    int compare_syscall_ret(Group *peer);
    int compare_timespan(Group *peer);
    int compare_wait(Group *peer);
    
    void decode_group(std::ofstream &outfile);
	void streamout_group(std::ofstream &output, EventBase *begin, EventBase *out);
    void streamout_group(std::ofstream &outfile);
    void streamout_group(std::ostream &outfile);
    void pic_group(std::ostream &outfile);
};
//////////////////////////////////////////////////////////////
typedef uint64_t group_id_t;

class ListsCategory {
public:
    typedef uint64_t opcode_t;
    typedef std::list<EventBase *> event_list_t;
    typedef std::map<tid_t, event_list_t> tid_evlist_t;
    typedef std::map<opcode_t, event_list_t> op_events_t;
protected:
    op_events_t &op_lists;
    tid_evlist_t tid_lists;

    pid_t tid2pid(uint64_t tid);
    void prepare_list_for_tid(tid_t tid, tid_evlist_t &);
    void threads_wait_graph(event_list_t &_list);

    ProcessInfo *get_process_info(tid_t tid, EventBase *front);
    static bool interrupt_mkrun_pair(EventBase *cur, event_list_t::reverse_iterator rit);
    int remove_sigprocessing_events(event_list_t &_list, event_list_t::reverse_iterator rit);

public:
    ListsCategory(op_events_t &_op_lists):op_lists(_op_lists) {
        tid_lists.clear();
        threads_wait_graph(op_lists[0]);
    }
    ~ListsCategory() {tid_lists.clear();/*TODO: delete fake woken events*/}
    void update_events_in_thread(uint64_t tid);
    op_events_t &get_op_lists(void){return op_lists;}
    tid_evlist_t &get_tid_lists(void) {return tid_lists;}
    event_list_t &get_list_of_tid(uint64_t tid) {return tid_lists[tid];}
    event_list_t &get_list_of_op(uint64_t op) {return op_lists[op];}
    event_list_t &get_wait_events(void) {return op_lists[MACH_WAIT];}
};

class ThreadType {
public:
    typedef enum {
        RunLoopThreadType,
        RunLoopThreadType_WITH_OBSERVER_ENTRY,
        RunLoopThreadType_WITHOUT_OBSERVER_ENTRY
    } thread_type_t;
    typedef std::map<thread_type_t, std::set<tid_t> > thread_category_t;
    typedef std::list<EventBase *> event_list_t;

protected:
    thread_category_t categories;
    tid_t main_thread;
    tid_t nsevent_thread;
public:
    ThreadType() {main_thread = nsevent_thread = -1; categories.clear();}
    ~ThreadType() {main_thread = nsevent_thread = -1; categories.clear();}
    void check_rlthreads(event_list_t &, event_list_t &);
    void check_host_uithreads(event_list_t &);
	bool is_runloop_thread(tid_t tid); 
    tid_t get_main_thread() {return main_thread;}
};

class Groups: public ListsCategory, public ThreadType {
public:
    typedef std::list<EventBase *> event_list_t;
    typedef std::map<group_id_t, Group *> gid_group_map_t;

private:
    gid_group_map_t groups;
    gid_group_map_t main_groups;
    gid_group_map_t host_groups;
    std::map<tid_t, gid_group_map_t> sub_results;

private:
    void init_groups();
    void para_identify_thread_types(void);
    void para_preprocessing_tidlists(void);
    void para_connector_generate(void);
	void para_thread_divide(void);
    void collect_groups(std::map<uint64_t, Group *> &sub_groups);

public:
    Groups(EventLists *eventlists_ptr);
    Groups(op_events_t &_op_lists);
    Groups(Groups &copy_groups);
    ~Groups(void);
    
    Group *group_of(EventBase *event);
    Group *spinning_group();
    Group *get_group_by_gid(uint64_t gid) {return groups.find(gid) != groups.end() ? groups[gid] : nullptr;}

    gid_group_map_t &get_groups(void) {return groups;}
    gid_group_map_t &get_main_groups() {return main_groups;}
    gid_group_map_t &get_host_groups() {return host_groups;}
    gid_group_map_t &get_groups_by_tid(uint64_t tid) {assert(sub_results.size() > 0); assert(sub_results.find(tid) != sub_results.end()); return sub_results[tid];}

    bool matched(Group *target);
    void partial_compare(Groups *peer_groups, std::string proc_name, std::string &output_path);
    bool check_interleaved_pattern(event_list_t &ev_list, event_list_t::iterator &it);
    void check_interleavemsg_from_thread(event_list_t &evlist);
    void check_msg_pattern();

    int decode_groups(std::string &output_path);
    int streamout_groups(std::string &output_path);
};

#endif
