#ifndef BACKTRACEINFO_HPP
#define BACKTRACEINFO_HPP

#include "base.hpp"
#include "loader.hpp"
using namespace std;

#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBTarget.h"
#include "lldb/API/SBModule.h"
#include "lldb/API/SBFunction.h"
#include "lldb/API/SBSymbol.h"
#include "lldb/API/SBSymbolContext.h"
#include "lldb/API/SBStream.h"
//#include "lldb/API/SBProcess.h"
//#include "lldb/API/SBListener.h"

extern "C" {
#include "symbolicator.h"
}

class Images;
class Frames;
typedef Images images_t;
typedef Frames frames_t;

typedef struct DebugData {
	lldb::SBDebugger debugger;
	lldb::SBTarget cur_target;
}debug_data_t;

class Images {
	string procname;
	pid_t pid;
	uint64_t tid;
	struct exec_path {
		uint64_t vm_offset;
		string path;
	} main_proc;
	map<string, uint64_t> modules_loaded_map;
public:
	Images(uint64_t tid, string procname);
	~Images(){modules_loaded_map.clear();}
	void set_pid(pid_t _pid);
	void set_main_proc(uint64_t vm_offset, string path) {main_proc.vm_offset = vm_offset; main_proc.path = path;}
	string get_main_proc_path(void) {return main_proc.path;}
	string& get_procname(void) {return procname;}
	pid_t get_pid(void) {return pid;}
	uint64_t get_tid() {return tid;}
	string search_path(uint64_t vm_addr);
	void add_module(uint64_t vm_offset, string path);
	map<string, uint64_t> & get_modules() {return modules_loaded_map;}
	void decode_images(ofstream &outfile);
};

typedef struct FrameInfo {
	uint64_t addr;
	string symbol;
	string filepath;
} frame_info_t;

class Frames {
	uint64_t host_event_tag;
	string proc_name;
	uint64_t tid;
	vector<uint64_t> frame_addrs;
	vector<string> frame_symbols;
	bool is_infected;
	bool is_spin;
	Images *image;
	static int32_t check_symtable;

public:
	Frames(uint64_t tag, string procname, uint64_t _tid);
	~Frames() {frame_addrs.clear(); frame_symbols.clear();}
	uint64_t get_tag(void) {return host_event_tag;}
	void add_frame(uint64_t frame) {frame_addrs.push_back(frame);}
	uint64_t get_size(void) {return frame_addrs.size();}
	vector<string> &get_symbols(void) {return frame_symbols;}
	bool check_symbol(string &func);
	bool check_infected(void) {return is_infected && is_spin;}
	void decode_frames(ofstream &outfile);
	void streamout(ofstream &outfile);

	/* below only referred when backtrace parser is valid */
	void set_image(Images *img) {image = img;}
	static string get_sym_for_addr(uint64_t vm_offset, map<uint64_t, string> & vm_sym_map);
	static void checking_symbol_with_image_in_memory(string &symbol, uint64_t vm, string path, map<string, map<uint64_t, string> >&, Images *);
	static bool lookup_symbol_via_lldb(debug_data_t * debugger_data, frame_info_t * cur_frame);
	void symbolication(debug_data_t * debugger, map<string, map<uint64_t, string> >&);
};

class BacktraceEvent: public EventBase {
	uint64_t host_event_tag;
	event_t *host_event;
	frames_t *frame_info;
	uint64_t max_frames;
public:
	BacktraceEvent(double abstime, string op, uint64_t tid, frames_t * frame_info, uint64_t max_frames, uint64_t host_event_tag, uint32_t core_id, string procname="");
	~BacktraceEvent(void);
	void add_frames(uint64_t * frames, int size);
	uint64_t frame_tag(void) {return frame_info->get_tag();}
	uint64_t get_size(void) {return frame_info->get_size();}
	uint64_t get_max_frames(void) {return max_frames;}
	vector<string> &get_symbols(void) {return frame_info->get_symbols();}
	bool hook_to_event(event_t *event, uint32_t event_type);
	event_t *get_hooked_event(void) {return host_event;}
	bool check_backtrace_symbol(string & func) {return frame_info->check_symbol(func);}
	bool check_infected(void) {return frame_info->check_infected();}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);

	/* below only referred when backtrace parser is valid */
	void connect_frame_with_image(Images *img) {frame_info->set_image(img);}
	void symbolize_frame(debug_data_t *debugger,  map<string, map<uint64_t, string> >&);
};

#endif
