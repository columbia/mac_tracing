#ifndef MKRUN_HPP
#define MKRUN_HPP

#include "base.hpp"

#define UNKNOWN_MR 		-1
#define WAKEUP_ALL		1
#define WAKEUP_ONE		2
#define WAKEUP_THR		3
#define CLEAR_WAIT		4

#define SCHED_TSM_MR    5
#define INTR_MR   		6
#define WORKQ_MR		7
#define CALLOUT_MR		8

class MkrunEvent : public EventBase {
	uint64_t peer_tid;
	uint64_t peer_wakeup_event;
	int64_t	peer_prio;
	uint64_t peer_wait_result;
	pid_t peer_pid;
	int32_t	mr_type;
	uint32_t peer_run_count;
	bool peer_ready_for_runq;
	wait_ev_t *wait;
public:
	MkrunEvent(double timestamp, string op, uint64_t tid, uint64_t peer_tid, uint64_t wakeup_event,
		uint64_t mr_type, pid_t pid, pid_t peer_pid, uint32_t coreid, string procname = "");
	void set_peer_prio(uint64_t _peer_prio) {peer_prio = _peer_prio;}
	void set_peer_wait_result(uint64_t wait_result) {peer_wait_result = wait_result;}
	void set_peer_run_count(uint32_t run_count) {peer_run_count = run_count;}
	void set_peer_ready_for_runq(uint32_t ready_for_runq) {peer_ready_for_runq = bool(ready_for_runq);}
	void set_mr_type(int32_t _mr_type) {mr_type = _mr_type;}
	uint64_t get_peer_tid(void) {return peer_tid;}
	pid_t get_peer_pid(void) {return peer_pid;}
	uint64_t get_peer_wakeup_event(void) {return peer_wakeup_event;}
	uint64_t get_peer_wait_result(void) {return peer_wait_result;}
	bool is_ready_for_runq(void) {return peer_ready_for_runq;}
	int32_t	get_mr_type(void) {return mr_type;}
	void pair_wait(wait_ev_t* _wait);
	wait_ev_t * get_wait(void) {return wait;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

#endif
