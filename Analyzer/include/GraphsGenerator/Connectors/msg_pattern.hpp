#ifndef MSG_PATTERN_HPP
#define MSG_PATTERN_HPP
#include "mach_msg.hpp"
#include <map>

typedef std::set<MsgEvent *> msg_episode;
typedef std::vector<MsgEvent *> msg_episode_v;

class MsgPattern {
    std::list<EventBase*> &ev_list;
    std::list<MsgEvent*> mig_list;
    std::list<MsgEvent*> msg_list;
	//build cache
	std::unordered_map<MsgEvent *, std::list<MsgEvent *>::iterator> itermap;
	std::unordered_map<MsgEvent *, bool> visit_map;
	struct key_t{
		uint64_t remote_port;
		//uint32_t remote_port_name;
		uint64_t local_port;
		//uint32_t local_port_name;
		bool is_recv;
		//uint32_t pid;

		bool operator == (const key_t &k) const
		{
			return remote_port == k.remote_port
				//&& remote_port_name == k.remote_port_name
				&& local_port == k.local_port
				//&& local_port_name == k.local_port_name
				&& is_recv == k.is_recv;
				//&& pid == k.pid;
		}
		
		bool operator < (const key_t &k) const
		{
			if (*this  == k) return false;

			if (remote_port != k.remote_port)
				return remote_port < k.remote_port;

			//if (remote_port_name != k.remote_port_name)
				//return remote_port_name < k.remote_port_name;

			if (local_port != k.local_port)
				return local_port < k.local_port;
						
			//if (local_port_name != k.local_port_name)
				//return local_port_name < k.local_port_name;

			//if (is_recv != k.is_recv)
			return is_recv < k.is_recv;

			//return pid < k.pid;
		}
	
		bool operator > (const key_t &k) const
		{
			return !(*this == k) && !(*this < k);
		}
		
	};

	std::multimap<key_t, MsgEvent *> msg_map;

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
