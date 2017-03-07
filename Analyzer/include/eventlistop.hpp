#ifndef EVENTLISTOP_HPP
#define EVENTLISTOP_HPP

#include "parser.hpp"
namespace EventListOp
{
	void sort_event_list(list<event_t*>&);
	void clear_event_list(list<event_t*> & evlist);
	void dump_all_event(list<event_t*> & evlist, const char *filepath);
	void streamout_all_event(list<event_t*> & evlist, const char *filepath);
	void streamout_backtrace(list<event_t*> & evlist, const char *filepath);
}
#endif
