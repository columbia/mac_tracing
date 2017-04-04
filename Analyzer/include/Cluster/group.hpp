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

#include "eventlistop.hpp"
#include "parse_helper.hpp"
#include <algorithm>

#include "msg_pattern.hpp"
#include "dispatch_pattern.hpp"
#include "timercall_pattern.hpp"
#include "mkrun_wait_pair.hpp"
#include "voucher_bank_attrs.hpp"

#define GROUP_ID_BITS 20

typedef struct {
	event_t * begin;
	event_t * end;
} state_boundary_t;

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

	map<string, state_boundary_t> state_boundary;
	map<string, double> state_aggregate_time;
	
public:
	Group(uint64_t _group_id, event_t *_root);
	~Group();
	void set_cluster_idx(uint64_t idx) {cluster_id = idx;}
	void set_group_id(uint64_t group_id);
	void set_group_msg_bank_holder(pid_t pid) {msg_bank_holder = pid;}
	void set_group_msg_peer(pid_t pid) {msg_peer = pid;}
	void add_group_peer(string proc_comm) {
		if (proc_comm.size())
			group_peer.insert(proc_comm);
	}
	void add_group_tags(vector<string> &desc);
	void add_group_tags(string desc);
	map<string, uint32_t> & get_group_tags(void) {return group_tags;}
	uint64_t get_cluster_idx(void) {return cluster_id;}
	uint64_t get_group_id(void) {return group_id;}
	void blockinvoke_level_inc(void) {blockinvoke_level++;}
	void blockinvoke_level_dec(void) {blockinvoke_level--;}
	uint64_t get_blockinvoke_level(void) {return blockinvoke_level;}
	pid_t get_group_msg_bank_holder(void) {return msg_bank_holder;}
	pid_t get_group_msg_peer(void) {return msg_peer;}
	bool check_ground(void) {return is_ground;}
	bool check_infected(void) {return is_infected;}
	void set_root(event_t *_root);
	event_t * get_root(void) {return root;}
	backtrace_ev_t * get_infected_event(void) {return infected_event;}
	uint64_t get_size(void) { return container.size();}
	void add_to_container(event_t*);
	list<event_t*> & get_container(void) {return container;}
	event_t * get_last_event(void) { return container.size() > 0 ? container.back() : NULL;}
	event_t * get_first_event(void) { return container.size() > 0 ? container.front() : NULL;}
	bool find_event(event_t * event) { return event->get_group_id() == group_id ? true: false;}
	void sort_container(void) {container.sort(Parse::EventComparator::compare_time);}
	void set_state_begin(string state, event_t * event) {state_boundary[state].begin = event;}
	void set_state_end(string state, event_t * event) {state_boundary[state].end = event;}
	event_t * get_state_begin(string state) {return state_boundary[state].begin;}
	event_t * get_state_end(string state) {return state_boundary[state].end;}

	void attribute_time(string state, double delta) {
		double new_val = state_aggregate_time[state] + delta;
		state_aggregate_time[state] = new_val;
	}
	double get_state_time(string state) {return state_aggregate_time[state];}
	void decode_group(ofstream &outfile);
	void streamout_group(ofstream &outfile);
};
typedef Group group_t;

class Groups {
	typedef map<uint64_t, list<event_t *> > tid_evlist_t;
	typedef map<uint64_t, list<event_t *> > op_events_t;
	typedef map<uint64_t, group_t*> gid_group_map_t;
	typedef map<mkrun_ev_t*, list<event_t *>::iterator> mkrun_pos_t;
	typedef map<uint64_t, string> tid_comm_map_t;
	typedef map<uint64_t, pid_t> tid_pid_map_t;
	typedef map<pid_t, string> pid_comm_map_t;

	op_events_t &op_lists;
	gid_group_map_t groups;
	uint64_t main_thread;
	uint64_t nsevent_thread;
	tid_evlist_t tid_lists;	
	vector<gid_group_map_t> sub_results;
	gid_group_map_t main_groups;

	// for update missing info
	tid_comm_map_t tid_comm;
	pid_comm_map_t pid_comm;
	tid_pid_map_t tid_pid;
	// for marking mkrun position
	mkrun_pos_t mkrun_map;

	pid_t tid2pid(uint64_t tid);
	string tid2comm(uint64_t tid);
	string pid2comm(pid_t pid);
	void update_procs_maps(mkrun_ev_t * mr_event);
	void update_procs_maps(wait_ev_t * wait_event);
	void update_procs_maps(event_t * event);
	void update_procs_maps(const char * proc_list_file);
	tid_evlist_t divide_eventlist_and_mapping(list<event_t *> &_list);

	void para_connector_generate(void); /* connect events for later clustering */
	void check_host_uithreads(list<event_t *> &);//backtrace_ev_t *backtrace_event);
	group_t * create_group(uint64_t group_id, event_t *root_event);
	bool voucher_manipulation(event_t *event);
	bool mr_by_intr(mkrun_ev_t * mr, intr_ev_t * intr_event);
	bool mr_by_wqnext(mkrun_ev_t * mr, event_t *last_event);
	uint64_t check_mr_type(mkrun_ev_t * mr_event, event_t * last_event, intr_ev_t * potential_root);
	bool check_group_with_voucher(voucher_ev_t * voucher, group_t * cur_group, pid_t msg_peer);
	group_t * add_mr_into_group(mkrun_ev_t * mr_event, group_t **cur_group_ptr, intr_ev_t * potential_root);
	group_t * add_wait_into_group(wait_ev_t * wait_event, group_t ** cur_group_ptr);
	group_t * add_invoke_into_group(blockinvoke_ev_t * invoke_event, group_t ** cur_group_ptr);

public:
	Groups(op_events_t &_op_lists);
	~Groups(void);
	map<uint64_t, list<event_t *> >& get_tid_lists(void) {return tid_lists;}
	list<event_t *> & get_list_of_tid(uint64_t tid) {return tid_lists[tid];}
	list<event_t *> & get_list_of_op(uint64_t op) {return op_lists[op];}
	list<event_t *> get_list_of_uimain(void) {
		if (main_thread == 0) {
			list<event_t *> empty;
			cerr << "Unable get main thread backtrace, abort" << endl;
			return empty;
		}
		return get_list_of_tid(main_thread);
	}
	list<event_t *> get_list_of_nsevent(void) {
		if (nsevent_thread == 0) {
			list<event_t *> empty;
			cerr << "Unable get nsevent thread backtrace, abort" << endl;
			return empty;
		}
		return get_list_of_tid(nsevent_thread);
	}
	mkrun_pos_t &get_all_mkrun(void) { return mkrun_map;}
	pid_comm_map_t & get_pid_comms(void) {return pid_comm;}
	map<uint64_t, group_t *> & get_groups(void) {return groups;}
	void para_group(void); /* parallelly grouping */
	void init_groups(int idx , list<event_t*> & tid_list); /* per-thread grouping */
	group_t * group_of(event_t *event);
	void collect_groups(map<uint64_t, group_t *> & sub_groups);
	map<uint64_t, group_t *> & get_main_groups() {return main_groups;}
	//void clear_groups(void);
	int decode_groups(string & output_path);
	int streamout_groups(string & output_path);
};
typedef Groups groups_t;

#endif
