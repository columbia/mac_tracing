#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -o hook_dispatch.dylib -dynamiclib hack_dispatch.c hook_dispatch.c patch_enqueue.c patch_dequeue.c patch_blockinvoke.c -include lib_mach_info.h -current_version 1.2.3 -compatibility_version 1.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.A.dylib

otool -l hook_dispatch.dylib | grep -A2 LC_REEXPORT_DYLIB
install_name_tool -change libRatings.A.dylib /usr/lib/system/orig.libdispatch.dylib hook_dispatch.dylib 
otool -l hook_dispatch.dylib | grep -A2 LC_REEXPORT_DYLIB
