#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -o hook_pthread.dylib -dynamiclib ../hack_lib/hack_lib.c patch_pthread_cond.c hook_pthread.c -include ../hack_lib/lib_mach_info.h -current_version 138.10.4 -compatibility_version 1.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.C.dylib

echo "Before"
otool -l hook_pthread.dylib | grep -A2 LC_REEXPORT_DYLIB
otool -l hook_pthread.dylib | grep -A2 LC_LOAD_DYLIB
install_name_tool -change libRatings.C.dylib /usr/lib/system/orig.libsystem_pthread.dylib hook_pthread.dylib 
echo "After"
otool -l hook_pthread.dylib | grep -A2 LC_REEXPORT_DYLIB
codesign -f -s "iPhone Developer: Lingmei Weng (FU6Q67TW73)" -v hook_pthread.dylib
