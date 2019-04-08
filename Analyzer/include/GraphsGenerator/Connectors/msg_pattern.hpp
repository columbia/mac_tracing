#ifndef MSG_PATTERN_HPP
#define MSG_PATTERN_HPP
#include "mach_msg.hpp"

typedef set<msg_ev_t *> msg_episode;
typedef vector<msg_ev_t *> msg_episode_v;

class MsgPattern {
	list<event_t*> &ev_list;
	list<msg_episode> patterned_ipcs;
	list<event_t*>::iterator search_ipc_msg(uint32_t *, pid_t *,
						uint64_t, uint64_t, bool,
						list<event_t *>::iterator, int *, bool *, uint32_t);
	list<event_t *>::iterator search_mig_msg(list<event_t *>::iterator, int *, bool *);
	void update_msg_pattern(msg_ev_t *, msg_ev_t *, msg_ev_t *, msg_ev_t *);
	list<msg_episode>::iterator episode_of(msg_ev_t*);
	vector<msg_ev_t *> sort_msg_episode(msg_episode & s);
	
public:
	MsgPattern(list<event_t *> &event_list);
	~MsgPattern(void);
	void collect_patterned_ipcs(void);
	list<msg_episode> & get_patterned_ipcs(void);
	void verify_msg_pattern(void);
	void decode_patterned_ipcs(string & output_path);
};

typedef MsgPattern msgpattern_t;
#endif
