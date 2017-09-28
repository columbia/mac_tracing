#!/bin/bash
#clang -arch x86_64 -arch i386 -Wall -o libdetour.A.dylib -dynamiclib hack_lib.c -include lib_mach_info.h -install_name /usr/lib/libdetour.A.dylib -compatibility_version 1.0.0 -current_version 1.0.0
clang hack_lib.c -include lib_mach_info.h -arch i386 -o check_sym

