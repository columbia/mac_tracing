#ifndef NSAPPEVENT_HPP
#define NSAPPEVENT_HPP

#include "base.hpp"
#include "eventref.hpp"

typedef enum {
    NSLeftMouseDown             = 1,            
    NSLeftMouseUp               = 2,
    NSRightMouseDown            = 3,
    NSRightMouseUp              = 4,
    NSMouseMoved                = 5,
    NSLeftMouseDragged          = 6,
    NSRightMouseDragged         = 7,
    NSMouseEntered              = 8,
    NSMouseExited               = 9,
    NSKeyDown                   = 10,
    NSKeyUp                     = 11,
    NSFlagsChanged              = 12,
    NSAppKitDefined             = 13,
    NSSystemDefined             = 14,
    NSApplicationDefined        = 15,
    NSPeriodic                  = 16,
    NSCursorUpdate              = 17,
    NSScrollWheel               = 22,
    NSTabletPoint               = 23,
    NSTabletProximity           = 24,
    NSOtherMouseDown            = 25,
    NSOtherMouseUp              = 26,
    NSOtherMouseDragged         = 27,
} nsapp_event_type_t;


class NSAppEventEvent:public EventBase {
    bool begin;
    uint64_t event_class;
    uint64_t key_code;
    CoreGraphicsRefEvent *event_ref;
public:
    NSAppEventEvent(double timestamp, std::string op, uint64_t tid, uint64_t begin, uint32_t event_core, std::string procname = "");
    void set_event(uint64_t _event_class, uint64_t _key_code) {
        //std::cerr << "set event class and keycode for nsappevent" << std::endl;
        event_class = _event_class;
        key_code = _key_code;
    }

    // pair to the nearest event_ref
    void set_event_ref(CoreGraphicsRefEvent *ref) {event_ref = ref;}
    CoreGraphicsRefEvent *get_event_ref(void) {return event_ref;}
    uint64_t get_event_class() {return event_class;}
    uint64_t get_key_code() {return key_code;}
    bool is_begin(void) {return begin;}
    const char *decode_event_type(int event_type);
    const char *decode_keycode(int key_code);
    std::string decode_event_to_string();
    void decode_event(bool is_verbose, std::ofstream &outfile);
    void streamout_event(std::ofstream &outfile);
};

#endif
