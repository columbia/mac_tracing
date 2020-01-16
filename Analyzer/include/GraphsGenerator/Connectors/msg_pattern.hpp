#ifndef MSG_PATTERN_HPP
#define MSG_PATTERN_HPP
#include "mach_msg.hpp"

typedef std::set<MsgEvent *> msg_episode;
typedef std::vector<MsgEvent *> msg_episode_v;

class MsgPattern {
    std::list<EventBase*> &ev_list;
    std::list<MsgEvent*> mig_list;
    std::list<MsgEvent*> msg_list;
    std::map<uint64_t, std::list<MsgEvent*> > local_port_msg_list_maps;

    std::list<msg_episode> patterned_ipcs;
    std::list<MsgEvent*>::iterator search_ipc_msg(
                uint32_t *, pid_t *, uint64_t,
                uint64_t, bool, 
                std::list<MsgEvent *>::iterator, uint32_t);

    void update_msg_pattern(MsgEvent *, MsgEvent *, MsgEvent *, MsgEvent *);
    std::list<msg_episode>::iterator episode_of(MsgEvent*);
    std::vector<MsgEvent *> sort_msg_episode(msg_episode & s);
    
public:
    MsgPattern(std::list<EventBase *> &event_list);
    //MsgPattern(std::list<MsgEvent *> &_mig_list, std::list<MsgEvent *> &_msg_list);
    ~MsgPattern(void);
    void collect_mig_pattern();
    void collect_msg_pattern();
    void collect_patterned_ipcs(void);
    std::list<msg_episode> &get_patterned_ipcs(void);
    void verify_msg_pattern(void);
    void decode_patterned_ipcs(std::string & output_path);
};

typedef MsgPattern msgpattern_t;
#endif
