#include <time.h>
#include <signal.h>
#include <execinfo.h>
#include <cstdio>
#include <unistd.h>
#include <boost/filesystem.hpp>
#include "argus_loop.h"

std::mutex mtx;
static void debug_handler(int sig)
{
    void *btarray[16];
    size_t size = backtrace(btarray, 16);
    std::cerr << "Signal " << sig << std::endl;
    backtrace_symbols_fd(btarray, size, STDERR_FILENO);
	if (sig != SIGSEGV)
  		exit(1);
	//mainloop();
}


int main(int argc, char* argv[]) {
    signal(SIGSEGV, debug_handler);
    signal(SIGABRT, debug_handler);
    signal(SIGINT, debug_handler);
	loguru::init(argc, argv);
	loguru::add_file("debugging_info", loguru::Truncate, loguru::Verbosity_INFO);
	ArgusLoop commands;
    mainloop(commands);
    return 0;
}
