#include "rlobserver.hpp"
RLObserverEvent::RLObserverEvent(double timestamp, string op, uint64_t tid, uint64_t _rl, uint64_t _mode, uint64_t _stage, uint32_t event_core, string procname)
:EventBase(timestamp, RL_OBSERVER_EVENT, op, tid, event_core, procname)
{
	rl = _rl;
	mode = _mode;
	stage = (rl_stage_t)_stage;
}

const char *RLObserverEvent::decode_activity_stage(uint64_t stage)
{
	switch (stage) {
		case kCFRunLoopEntry:
			return "RunLoopEntry";
		case kCFRunLoopExtraEntry:
			return "RunLoopExtraEntry";
		case kCFRunLoopBeforeTimers:
			return "RunLoopBeforeTimers";
		case kCFRunLoopBeforeSources:
			return "RunLoopBeforeSources";
		case kCFRunLoopBeforeWaiting:
			return "RunLoopBeforeWaiting";
		case kCFRunLoopAfterWaiting:
			return "RunLoopAfterWaiting";
		case kCFRunLoopExit:
			return "RunLoopExit";
		case kCFRunLoopAllActivities:
			return "AllActivities_Error";
		default:
			return "Error";
	}
}

void RLObserverEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\t" << decode_activity_stage(stage) << endl;
}

void RLObserverEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << decode_activity_stage(stage) << endl;
}
