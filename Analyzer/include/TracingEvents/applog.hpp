#ifndef APPLOG_HPP
#define APPLOG_HPP
#include "base.hpp"
class AppLogEvent: public EventBase
{
    std::string desc;

public:
    AppLogEvent(double timestamp, std::string op, uint64_t _tid, 
        std::string desc, uint32_t coreid, std::string procname);

	std::string get_prefix();
	std::string get_state();
    std::string get_desc() {return desc;}

    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
    void streamout_event(std::ostream &out);
};
#endif
