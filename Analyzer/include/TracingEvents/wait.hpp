#ifndef WAIT_HPP
#define WAIT_HPP

#include "base.hpp"
using namespace std;

#define THREAD_WAITING		   -1		/* thread is waiting */
#define THREAD_AWAKENED			0		/* normal wakeup */
#define THREAD_TIMED_OUT		1		/* timeout expired */
#define THREAD_INTERRUPTED		2		/* aborted/interrupted */
#define THREAD_RESTART			3		/* restart operation entirely */
#define THREAD_NOT_WAITING     10       /* thread didn't need to wait */

class WaitEvent : public EventBase {
	uint64_t wait_event;
	int	wait_result;
	string wait_resource;
	mkrun_ev_t * mkrun_event;

	const char* decode_wait_result(void);
public :
	WaitEvent(double timestamp, string op, uint64_t tid, uint64_t wait_event, uint32_t coreid, string procname = "");
	void pair_mkrun(mkrun_ev_t * mkrun) {mkrun_event = mkrun;}
	mkrun_ev_t * get_mkrun(void) {return mkrun_event;}
	void set_wait_result(int result) {wait_result = result;}
	void set_wait_resource(char * s) {wait_resource = s;}
	uint64_t get_wait_event(void) {return wait_event;}
	string get_wait_resource(void) {return wait_resource;}
	int get_wait_result(void) {return wait_result;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
