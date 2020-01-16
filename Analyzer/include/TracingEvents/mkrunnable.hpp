#ifndef MKRUN_HPP
#define MKRUN_HPP

#include "base.hpp"
#include "interrupt.hpp"

#define UNKNOWN_MR       -1

#define WAKEUP_ALL        0x0100
#define WAKEUP_ONE        0x0200
#define WAKEUP_THR        0x0300
#define CLEAR_WAIT        0x0400

//#define SCHED_TSM_MR      5
//#define INTR_MR           6
//#define WORKQ_MR          7
//#define CALLOUT_MR        8

class PeerThreadInfo {
    pid_t pid;
    uint64_t tid;

    uint64_t peer_wakeup_event;
    uint64_t peer_wait_result;
    int64_t  peer_prio;
    uint32_t peer_run_count;
    bool peer_ready_for_runq;
public:
    PeerThreadInfo();
    void set_peer_prio(uint64_t _peer_prio) {peer_prio = _peer_prio;}
    void set_peer_wait_result(uint64_t wait_result) {peer_wait_result = wait_result;}
    void set_peer_run_count(uint32_t run_count) {peer_run_count = run_count;}
    void set_peer_ready_for_runq(uint32_t ready_for_runq) {peer_ready_for_runq = bool(ready_for_runq);}
    uint64_t get_peer_wakeup_event(void) {return peer_wakeup_event;}
    uint64_t get_peer_wait_result(void) {return peer_wait_result;}
    bool is_ready_for_runq(void) {return peer_ready_for_runq;}
};


class MakeRunEvent: public EventBase {
    int32_t mr_type;
    pid_t peer_pid;
    tid_t peer_tid;
    WaitEvent *wait;
    uint64_t event_source;

public:
    static const int external = 0x10;
    static const int time_out = 0x11;
    static const int internal = 0x20;
    static const int workq = 0x21;
     
    MakeRunEvent(double timestamp, std::string op, uint64_t tid, uint64_t peer_tid, uint64_t _event_source,
        uint64_t mr_type, pid_t pid, pid_t peer_pid, uint32_t coreid, std::string procname = "");
    bool check_interrupt(IntrEvent *potential_root);
    bool is_timeout() {return (mr_type & time_out) == time_out;}
    bool is_external() {return (mr_type & external) == external;}
    uint64_t get_peer_tid(void) {return peer_tid;}
    void pair_wait(WaitEvent *_wait) {wait = _wait;}
    WaitEvent *get_wait(void) {return wait;}

    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};

class FakedWokenEvent: public EventBase {
public:
    FakedWokenEvent(double timestamp, std::string op, uint64_t tid, MakeRunEvent *mkrun_peer,
        uint32_t coreid, std::string procname = "");
    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};

#endif
