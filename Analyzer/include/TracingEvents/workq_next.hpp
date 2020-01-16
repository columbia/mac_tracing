#ifndef WORKQ_NEXT_HPP
#define WORKQ_NEXT_HPP

#include "base.hpp"

#define TIMER_BLOCK    0
#define NO_TIMER    1
#define NON_BLOCK  -1
class WorkQueueNextItemEvent : public EventBase {
    uint64_t workq;
    uint64_t thr;
    uint64_t thidlecount;
    uint64_t reqcount;
    uint64_t nextthr;
    bool is_reuse;
    bool is_over_commit;
    uint32_t block_type;
    uint32_t wqnext_type;
public:
    WorkQueueNextItemEvent(double timestamp, std::string op, uint64_t tid, uint64_t wq, uint64_t next_thread, uint64_t idle_count, uint64_t req_count, uint32_t coreid, std::string procname ="");
    void set_nextthr(uint64_t next_thread) {nextthr = next_thread;}
    void set_reuse(void) {is_reuse = true; nextthr = get_tid();}
    void set_run_mode(bool over_commit) {is_over_commit = over_commit;}
    void set_block_type(bool timer);
    void set_wqnext_type(uint32_t type) {wqnext_type = type;}
    uint64_t get_nextthr(void) {return nextthr;}
    bool check_reuse(void) {return is_reuse;}
    bool check_overcommit(void) {return is_over_commit;}
    uint32_t check_block_type(void) {return block_type;}
    uint32_t get_wqnext_type(void) {return wqnext_type;}
    uint64_t get_workq(void) {return workq;}
    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};

#endif
