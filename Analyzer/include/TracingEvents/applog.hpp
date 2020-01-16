#ifndef APPLOG_HPP
#define APPLOG_HPP
#include "base.hpp"
class AppLogEvent: public EventBase
{
    //uint64_t serial_num;
    std::string desc;
public:
    AppLogEvent(double timestamp, std::string op, uint64_t _tid, 
        std::string desc, uint32_t coreid, std::string procname);
        //uint64_t _serial_number, uint32_t coreid, std::string procname);
    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};
#endif
