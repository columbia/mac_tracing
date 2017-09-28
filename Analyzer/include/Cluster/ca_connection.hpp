#ifndef CA_CONNECTION_HPP
#define CA_CONNECTION_HPP
#include "caset.hpp"
#include "cadisplay.hpp"
class CAConnection {
	list<event_t *> &caset_list;
	list<event_t *> &cadisplay_list;
public:
	CAConnection(list<event_t *> &caset_list, list<event_t *> &cadisplay_list);
	void ca_connection(void);
};

typedef CAConnection ca_connection_t;
#endif
