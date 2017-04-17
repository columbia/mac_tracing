#!/bin/bash
clang -arch x86_64 -arch i386 -Wall -o libRatings.A.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 150.0.0 -current_version 1258.1.0
clang -arch x86_64 -arch i386 -Wall -o libRatings.C.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 1.0.0 -current_version 765.50.8
clang -arch x86_64 -arch i386 -Wall -o libRatings.B.dylib -dynamiclib Ratings.c -include Ratings.h -current_version 1258.1.0 -compatibility_version 150.0.0
