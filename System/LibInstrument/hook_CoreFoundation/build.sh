#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -dynamiclib ../hack_lib/hack_lib.c hook_CoreFoundation.c patch_CoreFoundation.c -include ../hack_lib/lib_mach_info.h -framework CoreFoundation -current_version 1258.1.0 -compatibility_version 150.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.A.dylib -o HookCoreFoundation

otool -l HookCoreFoundation | grep -A2 LC_REEXPORT_DYLIB
otool -l HookCoreFoundation | grep -A2 LC_LOAD_DYLIB

install_name_tool -change libRatings.A.dylib /System/Library/Frameworks/CoreFoundation.framework/Versions/A/orig.CoreFoundation HookCoreFoundation
install_name_tool -change /System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation /System/Library/Frameworks/CoreFoundation.framework/Versions/A/orig.CoreFoundation HookCoreFoundation

echo "After"
otool -l HookCoreFoundation | grep -A2 LC_REEXPORT_DYLIB
otool -l HookCoreFoundation | grep -A2 LC_LOAD_DYLIB
