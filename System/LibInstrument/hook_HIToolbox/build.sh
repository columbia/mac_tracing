#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -dynamiclib ../hack_lib/hack_lib.c hook_HIToolbox.m patch_HIToolbox.m -include ../hack_lib/lib_mach_info.h -framework Carbon -current_version 807.2.0 -compatibility_version 1.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.F.dylib -o HookHIToolbox

echo "Before"
otool -l HookHIToolbox | grep -A2 LC_REEXPORT_DYLIB
otool -l HookHIToolbox | grep -A2 LC_LOAD_DYLIB

install_name_tool -change libRatings.F.dylib /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox HookHIToolbox
install_name_tool -change /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/HIToolbox /System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox HookHIToolbox

echo "After"
otool -l HookHIToolbox | grep -A2 LC_REEXPORT_DYLIB
otool -l HookHIToolbox | grep -A2 LC_LOAD_DYLIB
codesign -f -s "iPhone Developer: Lingmei Weng (FU6Q67TW73)" -v HookHIToolbox
