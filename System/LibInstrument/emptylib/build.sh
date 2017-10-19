#!/bin/bash
#clang -arch x86_64 -arch i386 -Wall -o libRatings.A.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 150.0.0 -current_version 1258.1.0
clang -arch x86_64 -arch i386 -Wall -o libRatings.B.dylib -dynamiclib Ratings.c -include Ratings.h -current_version 600.0.0 -compatibility_version 64.0.0
#clang -arch x86_64 -arch i386 -Wall -o libRatings.C.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 1.0.0 -current_version 765.50.8

#clang -arch x86_64 -arch i386 -Wall -o libRatings.D.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 45.0.0 -current_version 1504.81.100
##clang -arch x86_64 -arch i386 -Wall -o libRatings.E.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 45.0.0 -current_version 1504.81.100

#clang -arch x86_64 -arch i386 -Wall -o libRatings.F.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 1.0.0 -current_version 807.2.0
#clang -arch x86_64 -arch i386 -Wall -o libRatings.G.dylib -dynamiclib Ratings.c -include Ratings.h -compatibility_version 1.2.0 -current_version 1.11.0
