#include "parser.hpp"
#include "applog.hpp"

Parse::AppLogParser::AppLogParser(std::string filename)
:Parser(filename)
{
}

void Parse::AppLogParser::process()
{
    std::string line, deltatime, opname, procname;
    double abstime;
    bool is_begin;
    //uint64_t serial_num, unused, tid, coreid;
    uint64_t array[4];
    uint64_t tid, coreid;

    while (getline(infile, line)) {
        std::istringstream iss(line);
        if (!(iss >> abstime >> deltatime >> opname >> std::hex >> array[0] >> array[1] >> array[2] >> array[3]\
             >> tid >> coreid)) {
            outfile << line << std::endl;
            continue;
        }
        std::string desc = Parse::QueueOps::array_to_string(array, 4);
        if (!getline(iss >> std::ws, procname) || !procname.size())
            procname = ""; 
        AppLogEvent *applog_event = new AppLogEvent(abstime, opname, tid, desc, coreid, procname);
        local_event_list.push_back(applog_event);
    }
}
