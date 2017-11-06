export LANG=en_US.US-ASCII
mkdir -p obj/Release
mkdir -p obj/x86_64

SDKPATH=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk
SRCPATH=/Users/weng/Documents/Beachball/trace_proj

clang -isysroot $SDKPATH -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I$SRCPATH/imported_obj/trace.hmap -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -Wall -Wcast-align -F$SRCPATH/obj/Release -MMD -MT dependencies -MF $SRCPATH/imported_obj/Objects-normal/x86_64/trace.d -c $SRCPATH/trace.tproj/trace.c -o $SRCPATH/obj/x86_64/trace.o

clang++ -x c++ -stdlib=libc++ -isysroot $SDKPATH -fasm-blocks -fstrict-aliasing -Wdeprecated-declarations -mmacosx-version-min=10.11 -g -fvisibility=hidden -Wno-sign-conversion -I$SRCPATH/imported_obj/trace.hmap -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders -I$SDKPATH/System/Library/Frameworks/System.framework/PrivateHeaders/bsd -Wall -Wcast-align -F$SRCPATH/obj/Release -MMD -MT dependencies -MF $SRCPATH/imported_obj/Objects-normal/x86_64/trace.d -c $SRCPATH/trace.tproj/maps.cpp -o $SRCPATH/obj/x86_64/maps.o

clang++ -arch x86_64 -isysroot $SDKPATH -L$SRCPATH/obj/Release -F$SRCPATH/obj/Release -filelist $SRCPATH/imported_obj/Objects-normal/x86_64/trace.LinkFileList -mmacosx-version-min=10.11 -lutil -Xlinker -dependency_info -Xlinker $SRCPATH/imported_obj/Objects-normal/x86_64/trace_dependency_info.dat -o $SRCPATH/obj/Release/trace

cp obj/Release/trace ../utils/ring_trace
mkdir -p ~/trace_logs/
cp obj/Release/trace ~/trace_logs/ring_trace
