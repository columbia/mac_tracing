#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib/ -o hook_machmsg.dylib -dynamiclib ../hack_lib/hack_lib.c hook_machmsg.c patch_machmsg.c -include ../hack_lib/lib_mach_info.h -current_version 3284.60.10 -compatibility_version 1.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.C.dylib

otool -l hook_machmsg.dylib | grep -A2 LC_REEXPORT_DYLIB

install_name_tool -change libRatings.C.dylib /usr/lib/system/orig.libsystem_kernel.dylib hook_machmsg.dylib 

otool -l hook_machmsg.dylib | grep -A2 LC_REEXPORT_DYLIB

