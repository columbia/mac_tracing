#ifndef GROUP_HPP
#define GROUP_HPP
#include "base.hpp"
#include "mach_msg.hpp"
#include "mkrun.hpp"
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
#include <algorithm>

#include "msg_pattern.hpp"
#include "dispatch_pattern.hpp"
#include "timercall_pattern.hpp"
#include "mkrun_wait_pair.hpp"
#include "voucher_bank_attrs.hpp"
#include "ca_connection.hpp"
#include "nsappevent.hpp"
#include "breakpoint_trap_connection.hpp"
#include "rl_connection.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

#define GROUP_ID_BITS 20

#define RLTHR				0
#define RLTHR_WITH_OBSERVER_ENTRY	1
#define RLTHR_WO_OBSERVER_ENTRY		2
#define WQTHR				3

class Group;
class Groups;
typedef uint64_t tid_t;
typedef uint64_t opcode_t;
typedef uint64_t group_id_t;
typedef uint64_t thread_type_t;
typedef Group group_t;

typedef map<tid_t, list<event_t *> > tid_evlist_t;
typedef map<opcode_t, list<event_t *> > op_events_t;
typedef map<thread_type_t, set<tid_t> > thread_category_t;
typedef map<group_id_t, group_t *> gid_group_map_t;

class Group {
	uint64_t cluster_id;
	group_id_t group_id;
	uint64_t blockinvoke_level;
	pid_t msg_bank_holder;
	pid_t msg_peer;
	event_t *root;
	event_t *nsapp_event;
	list<event_t *> container;
	map<string, uint32_t> group_tags;
	bool sorted;
	set<string> group_peer;
	double time_span;
	
public:
	Group(uint64_t _group_id, event_t *_root);
	Group(Group const &g) {*this = g;}
	~Group();

	void set_cluster_idx(uint64_t idx) {cluster_id = idx;}
	uint64_t get_cluster_idx(void) {return cluster_id;}

	void set_group_id(group_id_t group_id);
	group_id_t get_group_id(void) {return group_id;}

	//add info to aid dividing
	void set_group_msg_bank_holder(pid_t pid) {msg_bank_holder = pid;}
	pid_t get_group_msg_bank_holder(void) {return msg_bank_holder;}
	void set_group_msg_peer(pid_t pid) {msg_peer = pid;}
	pid_t get_group_msg_peer(void) {return msg_peer;}
	void add_group_peer(string proc_comm);
	void add_group_tags(vector<string> &desc);
	void add_group_tags(string desc);
	map<string, uint32_t> &get_group_tags(void) {return group_tags;}

	void blockinvoke_level_inc(void) {blockinvoke_level++;}
	void blockinvoke_level_dec(void) {blockinvoke_level--;}
	uint64_t get_blockinvoke_level(void) {return blockinvoke_level;}
	//end of add info to aid dividing

	event_t *contain_nsappevent(void) {return nsapp_event;}
	void set_root(event_t *root);
	event_t *get_root(void) {return root;}

	void add_to_container(event_t *);
	void add_to_container(Group *);
	void empty_container(void);
	list<event_t *> &get_container(void) {return container;}
	void sort_container(void) {
		if (sorted == false) {
			container.sort(Parse::EventComparator::compare_time);
			sorted = true;
		}
	}
	event_t *get_last_event(void) {
		sort_container();
		return container.size() > 0 ? container.back() : NULL;
	}
	event_t *get_first_event(void) {
		sort_container();
		return container.size() > 0 ? container.front() : NULL;
	}
	tid_t get_tid(void) {return get_first_event()->get_tid();}
	uint64_t get_size(void) {return container.size();}

	bool find_event(event_t *event) { return event->get_group_id() == group_id ? true: false;}
	bool operator==(Group &peer);
	double calculate_time_span(void);
	/*
	 * 0; if equal
	 * 1; if this > peer if applicable
	 * -1: if this < peer if applicable
	*/
	int compare_syscall_ret(group_t *peer);
	int compare_timespan(group_t *peer);
	int compare_wait(group_t *peer);
	bool contains_noncausual_mk_edge();
	bool wait_over();
	
	void decode_group(ofstream &outfile);
	void streamout_group(ofstream &outfile);
	void streamout_group(ostream &outfile);
	//void streamout_group(ofstream &outfile);
	void pic_group(ostream &outfile);
};

class Groups {
	op_events_t &op_lists;
	tid_evlist_t tid_lists;	
	thread_category_t categories;
	tid_t main_thread;
	tid_t nsevent_thread;

	gid_group_map_t groups;
	gid_group_map_t main_groups;
	gid_group_map_t host_groups;

	map<tid_t, gid_group_map_t> sub_results;
	void init_groups();

	/* classify and recognize threads */
	tid_evlist_t divide_eventlist_and_mapping(list<event_t *> &_list);
	void check_host_uithreads(list<event_t *> &);
	void check_wqthreads(list<event_t *> &);
	void check_rlthreads(list<event_t *> &, list<event_t *> &);
	int remove_sigprocessing_events(list<event_t *> &_list, list<event_t *>::reverse_iterator rit);
	void update_events_in_thread(uint64_t tid);
	void para_preprocessing_tidlists(void);

	/*para connector gnerate*/
	void para_connector_generate(void);

	/*grouping*/
	void collect_groups(map<uint64_t, group_t *> &sub_groups);
	void para_group(void); 

public:
	Groups(EventLists *eventlists_ptr);
	Groups(op_events_t &_op_lists);
	Groups(Groups &copy_groups);
	~Groups(void);

	static pid_t tid2pid(uint64_t tid);
	static string tid2comm(uint64_t tid);
	static string pid2comm(pid_t pid);

	static bool interrupt_mkrun_pair(event_t *cur, list<event_t*>::reverse_iterator rit);
	op_events_t &get_op_lists(void){return op_lists;}
	map<tid_t, list<event_t *> > &get_tid_lists(void) {return tid_lists;}
	list<event_t *> &get_list_of_tid(uint64_t tid) {return tid_lists[tid];}
	list<event_t *> &get_list_of_op(uint64_t op) {return op_lists[op];}
	list<event_t *> &get_wait_events(void) {return op_lists[MACH_WAIT];}

	group_t *group_of(event_t *event);
	tid_t get_main_thread() {return main_thread;}
	gid_group_map_t &get_groups(void) {return groups;}
	gid_group_map_t &get_main_groups() {return main_groups;}
	gid_group_map_t &get_host_groups() {return host_groups;}
	gid_group_map_t &get_groups_by_tid(uint64_t tid) {return sub_results[tid];}
	group_t *get_group_by_gid(uint64_t gid) {return groups.find(gid) != groups.end() ? groups[gid] : NULL;}

	group_t *spinning_group();
	bool matched(group_t *target);
	void partial_compare(Groups *peer_groups, string proc_name, string &output_path);

	/*over connection checking via mach message (packed messages on mach_msg_trap/mach_msg_overwrite_trap)*/
	//bool check_interleaved_pattern(list<event_t *> &ev_list, list<event_t *>::iterator &it, ofstream output);
	//void check_interleavemsg_from_thread(list<event_t *> &evlist, ofstream &output);
	bool check_interleaved_pattern(list<event_t *> &ev_list, list<event_t *>::iterator &it);
	void check_interleavemsg_from_thread(list<event_t *> &evlist);

	//void check_pattern(string filename);
	void check_noncausual_mkrun();
	void check_msg_pattern();

	int decode_groups(string &output_path);
	int streamout_groups(string &output_path);
};
typedef Groups groups_t;

#endif
