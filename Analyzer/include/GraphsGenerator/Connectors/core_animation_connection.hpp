#ifndef CA_CONNECTION_HPP
#define CA_CONNECTION_HPP
#include "caset.hpp"
#include "cadisplay.hpp"
class CoreAnimationConnection {
    std::list<EventBase *> &caset_list;
    std::list<EventBase *> &cadisplay_list;
public:
    CoreAnimationConnection(std::list<EventBase *> &caset_list, std::list<EventBase *> &cadisplay_list);
    void core_animation_connection(void);
};

#endif
