#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -dynamiclib ../hack_lib/hack_lib.c hook_QuartzCore.m patch_QuartzCore.m -include ../hack_lib/lib_mach_info.h -framework QuartzCore -current_version 1.11.0 -compatibility_version 1.2.0 -Xlinker -reexport_library /usr/local/lib/libRatings.G.dylib -o HookQuartzCore

echo "Before"
otool -l HookQuartzCore | grep -A2 LC_REEXPORT_DYLIB
otool -l HookQuartzCore | grep -A2 LC_LOAD_DYLIB

install_name_tool -change libRatings.G.dylib /System/Library/Frameworks/QuartzCore.framework/Versions/A/orig.QuartzCore HookQuartzCore
install_name_tool -change /System/Library/Frameworks/QuartzCore.framework/Versions/A/QuartzCore /System/Library/Frameworks/QuartzCore.framework/Versions/A/orig.QuartzCore HookQuartzCore

echo "After"
otool -l HookQuartzCore | grep -A2 LC_REEXPORT_DYLIB
otool -l HookQuartzCore | grep -A2 LC_LOAD_DYLIB
codesign -f -s "iPhone Developer: Lingmei Weng (FU6Q67TW73)" -v HookQuartzCore
