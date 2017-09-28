#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -o hook_dispatch.dylib -dynamiclib ../hack_lib/hack_lib.c hook_dispatch.c patch_enqueue.c patch_dequeue.c patch_blockinvoke.c -include ../hack_lib/lib_mach_info.h -current_version 1.2.3 -compatibility_version 1.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.C.dylib
echo "Before"
otool -l hook_dispatch.dylib | grep -A2 LC_REEXPORT_DYLIB
install_name_tool -change libRatings.C.dylib /usr/lib/system/orig.libdispatch.dylib hook_dispatch.dylib 
echo "After"
otool -l hook_dispatch.dylib | grep -A2 LC_REEXPORT_DYLIB
codesign -f -s "iPhone Developer: Lingmei Weng (FU6Q67TW73)" -v hook_dispatch.dylib
