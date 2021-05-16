#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -I ../hack_lib -dynamiclib ../hack_lib/hack_lib.c hook_AppKit.m patch_AppKit.m -include ../hack_lib/lib_mach_info.h -framework AppKit -current_version 1504.81.100 -compatibility_version 45.0.0 -Xlinker -reexport_library /usr/local/lib/libRatings.D.dylib -o HookAppKit

echo "Before"
otool -l HookAppKit | grep -A2 LC_REEXPORT_DYLIB
otool -l HookAppKit | grep -A2 LC_LOAD_DYLIB

install_name_tool -change libRatings.D.dylib /System/Library/Frameworks/AppKit.framework/Versions/C/orig.AppKit HookAppKit
install_name_tool -change /System/Library/Frameworks/AppKit.framework/Versions/C/AppKit /System/Library/Frameworks/AppKit.framework/Versions/C/orig.AppKit HookAppKit

echo "After"
otool -l HookAppKit | grep -A2 LC_REEXPORT_DYLIB
otool -l HookAppKit | grep -A2 LC_LOAD_DYLIB
