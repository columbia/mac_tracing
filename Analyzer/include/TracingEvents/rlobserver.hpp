#ifndef RLOBSERVER_HPP
#define RLOBSERVER_HPP

#include "base.hpp"

/* Run Loop Observer Activities
#define kCFRunLoopEntry  (1UL << 0)
#define kCFRunLoopBeforeTimers   (1UL << 1)
#define kCFRunLoopBeforeSources   (1UL << 2)
#define kCFRunLoopBeforeWaiting   (1UL << 5)
#define kCFRunLoopAfterWaiting   (1UL << 6)
#define kCFRunLoopExit   (1UL << 7)
#define kCFRunLoopAllActivities   0x0FFFFFFFU
*/

typedef enum {
    kCFRunLoopEntry = (1UL << 0),
    kCFRunLoopBeforeTimers = (1UL << 1),
    kCFRunLoopBeforeSources = (1UL << 2),
    kCFRunLoopBeforeWaiting = (1UL << 5),
    kCFRunLoopAfterWaiting = (1UL << 6),
    kCFRunLoopExit = (1UL << 7),
    kCFRunLoopExtraEntry = (1UL << 8),
    kCFRunLoopAllActivities = 0x0FFFFFFFUL,
} rl_stage_t;

class RunLoopObserverEvent:public EventBase {
    uint64_t rl;
    uint64_t mode;
    rl_stage_t stage;
public:
    RunLoopObserverEvent(double timestamp, std::string op, uint64_t tid, uint64_t _rl, uint64_t _mode, uint64_t _stage, uint32_t event_core, std::string procname = "");
    rl_stage_t get_stage(void) {return stage;}
    const char *decode_activity_stage(uint64_t stage);
    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};
#endif
