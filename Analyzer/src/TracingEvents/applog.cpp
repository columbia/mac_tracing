#include "applog.hpp"
AppLogEvent::AppLogEvent(double timestamp, std::string op, uint64_t _tid, 
    std::string _desc,
    //uint64_t _serial_number,
    uint32_t _coreid, std::string procname)
:EventBase(timestamp, APPLOG_EVENT, op, _tid, _coreid, procname)
{
    //serial_num = _serial_number;
    desc = _desc;
}

void AppLogEvent::decode_event(bool is_verbose, std::ofstream &outfile)
{
}

void AppLogEvent::streamout_event(std::ofstream &outfile)
{
    EventBase::streamout_event(outfile);
    outfile << "\t" << desc << std::endl;
    //outfile << "\t" << std::hex << serial_num << std::endl;
}

