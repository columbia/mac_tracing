export LANG=en_US.US-ASCII
mkdir -p Release

#/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -x c -arch x86_64 -fmessage-length=178 -fdiagnostics-show-note-include-stack -fmacro-backtrace-limit=0 -fcolor-diagnostics -Wno-trigraphs -fpascal-strings -Os -Wno-missing-field-initializers -Wmissing-prototypes -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wno-unused-variable -Wunused-value -Wno-empty-body -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-conversion -Wno-constant-conversion -Wno-int-conversion -Wno-bool-conversion -Wno-enum-conversion -Wshorten-64-to-32 -Wpointer-sign -Wno-newline-eof  -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/trace.hmap -I/Users/weng/Documents/Beachball/trace_proj/build/Release/include -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/System.framework/PrivateHeaders -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/DerivedSources/x86_64 -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/DerivedSources -Wall -Wcast-align -F/Users/weng/Documents/Beachball/trace_proj/build/Release -MMD -MT dependencies -MF /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.d --serialize-diagnostics /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.dia -c /Users/weng/Documents/Beachball/trace_proj/trace.tproj/trace.c -o /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.o
#/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ -x c++ -stdlib=libc++ -arch x86_64 -fmessage-length=178 -fdiagnostics-show-note-include-stack -fmacro-backtrace-limit=0 -fcolor-diagnostics -Wno-trigraphs -fpascal-strings -Os -Wno-missing-field-initializers -Wmissing-prototypes -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wno-unused-variable -Wunused-value -Wno-empty-body -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-conversion -Wno-constant-conversion -Wno-int-conversion -Wno-bool-conversion -Wno-enum-conversion -Wshorten-64-to-32 -Wpointer-sign -Wno-newline-eof -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/trace.hmap -I/Users/weng/Documents/Beachball/trace_proj/build/Release/include -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/System.framework/PrivateHeaders -I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/DerivedSources/x86_64 -I/Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/DerivedSources -Wall -Wcast-align -F/Users/weng/Documents/Beachball/trace_proj/build/Release -MMD -MT dependencies -MF /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.d --serialize-diagnostics /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.dia -c /Users/weng/Documents/Beachball/trace_proj/trace.tproj/maps.cpp -o /Users/weng/Documents/Beachball/trace_proj/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/maps.o

#SDKPATH=\/Applications\/Xcode.app\/Contents\/Developer\/Platforms\/MacOSX.platform\/Developer\/SDKs\/MacOSX10.11.sdk
#SRCPATH=\/Users\/weng\/Documents\/Beachball\/trace_proj

SDKPATH=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk
SRCPATH=/Users/weng/Documents/Beachball/trace_proj

clang -isysroot $SDKPATH -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I$SRCPATH/build/system_cmds.build/Release/trace.build/trace.hmap -I$SRCPATH/build/Release/include -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -I$SRCPATH/build/system_cmds.build/Release/trace.build/DerivedSources/x86_64 -I$SRCPATH/build/system_cmds.build/Release/trace.build/DerivedSources -Wall -Wcast-align -F$SRCPATH/build/Release -MMD -MT dependencies -MF $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.d --serialize-diagnostics $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.dia -c $SRCPATH/trace.tproj/trace.c -o $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.o

clang++ -x c++ -stdlib=libc++ -isysroot $SDKPATH -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I$SRCPATH/build/system_cmds.build/Release/trace.build/trace.hmap -I$SRCPATH/build/Release/include -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -I$SRCPATH/build/system_cmds.build/Release/trace.build/DerivedSources/x86_64 -I$SRCPATH/build/system_cmds.build/Release/trace.build/DerivedSources -Wall -Wcast-align -F$SRCPATH/build/Release -MMD -MT dependencies -MF $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.d --serialize-diagnostics $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.dia -c $SRCPATH/trace.tproj/maps.cpp -o $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/maps.o

clang++ -arch x86_64 -isysroot $SDKPATH -L$SRCPATH/build/Release -F$SRCPATH/build/Release -filelist $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace.LinkFileList -mmacosx-version-min=10.11 -lutil -Xlinker -dependency_info -Xlinker $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/trace_dependency_info.dat $SRCPATH/build/system_cmds.build/Release/trace.build/Objects-normal/x86_64/maps.o -o $SRCPATH/build/Release/trace

mkdir -p ~/trace_logs/
cp build/Release/trace ~/trace_logs/ring_trace
