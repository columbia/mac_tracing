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
#include "timer_callout.hpp"
#include "voucher.hpp"
#include "backtraceinfo.hpp"
#include "rlobserver.hpp"
#include "breakpoint_trap.hpp"
#include "cadisplay.hpp"
#include "caset.hpp"

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

#define GROUP_ID_BITS 20

#define RLTHR_WITH_OBSERVER_ENTRY 0
#define RLTHR_WO_OBSERVER_ENTRY	1

class Group {
	uint64_t cluster_id;
	uint64_t group_id;
	uint64_t blockinvoke_level;
	bool is_ground;
	bool is_infected;
	pid_t msg_bank_holder;
	pid_t msg_peer;
	event_t * root;
	backtrace_ev_t * infected_event;
	list<event_t *> container;
	map<string, uint32_t> group_tags;
	set<string> group_peer;
	
public:
	Group(uint64_t _group_id, event_t *_root);
	~Group();
	void set_cluster_idx(uint64_t idx) {cluster_id = idx;}
	void set_group_id(uint64_t group_id);
	void set_group_msg_bank_holder(pid_t pid) {msg_bank_holder = pid;}
	void set_group_msg_peer(pid_t pid) {msg_peer = pid;}
	void add_group_peer(string proc_comm);
	void add_group_tags(vector<string> &desc);
	void add_group_tags(string desc);
	map<string, uint32_t> &get_group_tags(void) {return group_tags;}
	uint64_t get_cluster_idx(void) {return cluster_id;}
	uint64_t get_group_id(void) {return group_id;}
	void blockinvoke_level_inc(void) {blockinvoke_level++;}
	void blockinvoke_level_dec(void) {blockinvoke_level--;}
	uint64_t get_blockinvoke_level(void) {return blockinvoke_level;}
	pid_t get_group_msg_bank_holder(void) {return msg_bank_holder;}
	pid_t get_group_msg_peer(void) {return msg_peer;}
	bool check_ground(void) {return is_ground;}
	bool check_infected(void) {return is_infected;}
	void set_root(event_t *root);
	event_t *get_root(void) {return root;}
	backtrace_ev_t *get_infected_event(void) {return infected_event;}
	uint64_t get_size(void) {return container.size();}
	void add_to_container(event_t *);
	void add_to_container(Group *);
	void empty_container(void);
	list<event_t *> &get_container(void) {return container;}
	void sort_container(void) {container.sort(Parse::EventComparator::compare_time);}
	event_t *get_last_event(void) { return container.size() > 0 ? container.back() : NULL;}
	event_t *get_first_event(void) { return container.size() > 0 ? container.front() : NULL;}
	bool find_event(event_t *event) { return event->get_group_id() == group_id ? true: false;}

	void decode_group(ofstream &outfile);
	void streamout_group(ofstream &outfile);
	void pic_group(ofstream &outfile);
};
typedef Group group_t;

class Groups {
	typedef map<uint64_t, list<event_t *> > tid_evlist_t;
	typedef map<uint64_t, list<event_t *> > op_events_t;
	typedef map<uint64_t, set<uint64_t> > thread_category_t;
	typedef map<uint64_t, group_t *> gid_group_map_t;
	typedef map<mkrun_ev_t *, list<event_t *>::iterator> mkrun_pos_t;
	

	op_events_t &op_lists;
	gid_group_map_t groups;
	tid_evlist_t tid_lists;	
	thread_category_t categories;
	uint64_t main_thread;
	uint64_t nsevent_thread;

	vector<gid_group_map_t> sub_results;
	gid_group_map_t main_groups;
	mkrun_pos_t mkrun_map; // for marking mkrun position

	pid_t tid2pid(uint64_t tid);
	string tid2comm(uint64_t tid);
	string pid2comm(pid_t pid);
	bool interrupt_mkrun_pair(event_t *cur, list<event_t*>::reverse_iterator rit);
	bool remove_sigprocessing_events(list<event_t *> &event_list);
	tid_evlist_t divide_eventlist_and_mapping(list<event_t *> &_list);
	void check_host_uithreads(list<event_t *> &);
	void check_rlthreads(list<event_t *> &);
	void para_connector_generate(void);	// connect events for later clustering
	void mr_connector_generate(void);	// connect mr events cross thread and event types

public:
	Groups(op_events_t &_op_lists);
	~Groups(void);
	map<uint64_t, list<event_t *> > &get_tid_lists(void) {return tid_lists;}
	list<event_t *> &get_list_of_tid(uint64_t tid) {return tid_lists[tid];}
	list<event_t *> &get_list_of_op(uint64_t op) {return op_lists[op];}

	//void divide(int idx , list<event_t*> &tid_list); /* per-thread grouping */
	void para_group(void); /* parallelly grouping */
	group_t *group_of(event_t *event);
	void collect_groups(map<uint64_t, group_t *> &sub_groups);
	map<uint64_t, group_t *> &get_main_groups() {return main_groups;}
	map<uint64_t, group_t *> &get_groups(void) {return groups;}
	mkrun_pos_t &get_all_mkrun(void) { return mkrun_map;}

	bool check_interleaved_pattern(list<event_t *> &ev_list, list<event_t *>::iterator &it);
	void check_pattern(string filename);

	int decode_groups(string &output_path);
	int streamout_groups(string &output_path);
	
};
typedef Groups groups_t;

#endif
