TARGET		:= pattern_check analyzer who_trigger_spinning
#trace_voucher diff
CXX			:= clang++
CC			:= clang
CXXFLAGS	:= -c -Wall -std=c++11 -stdlib=libc++ 
CFLAGS		:= -c -Wall
LDFLAGS		:= -Xlinker /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/LLDB \
			-L/usr/local/Cellar/boost/1.63.0/lib \
			-lboost_regex-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_system-mt

INC_LLDB	:= -I/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Headers/
INC_BOOST	:= -I/usr/local/Cellar/boost/1.63.0/include/
INC_DIR		:= -I$(PWD)/include/
INC_EVENTS	:= -I$(PWD)/include/TracingEvents
INC_CLUSTER	:= -I$(PWD)/include/Cluster -I$(PWD)/include/Cluster/Connectors -I$(PWD)/include/Cluster/Voucher_trace
INCLUDES	:= $(INC_LLDB) $(INC_BOOST) $(INC_DIR) $(INC_EVENTS) $(INC_CLUSTER)

MODULES		:= TracingEvents Loader Parser Cluster Cluster/Connectors Cluster/Voucher_trace Canonization
SRC_DIR		:= $(addprefix $(PWD)/src/, $(MODULES))
OBJ_DIR		:= $(PWD)/obj $(addprefix $(PWD)/obj/, $(MODULES))
BIN_DIR		:= $(PWD)/bin/
CXX_SOURCES := $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.cpp))
C_SOURCES 	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.c))
CXX_OBJECTS := $(patsubst $(PWD)/src/%.cpp, $(PWD)/obj/%.o, $(CXX_SOURCES))
C_OBJECTS 	:= $(patsubst $(PWD)/src/%.c, $(PWD)/obj/%.o, $(C_SOURCES))
EXECUTABLE	:= $(addprefix $(BIN_DIR), $(TARGET))

vpath %.cpp $(SRC_DIR) $(PWD)/src/
vpath %.c $(SRC_DIR) $(PWD)/src/

define make-goal-cxx
$1/%.o: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $$< -o $$@
endef

define make-goal-c
$1/%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS) $$< -o $$@
endef

.PHONY:directories

all: directories $(EXECUTABLE)

directories: $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

define make-target
$1: $(PWD)/obj/$(notdir $1).o $(CXX_OBJECTS) $(C_OBJECTS)
	$(CXX) $(INCLUDES) $(CXX_OBJECTS) $(C_OBJECTS) $(LDFLAGS) $$< -o $$@ 
	install_name_tool -change @rpath/LLDB.framework/LLDB \
	/Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/LLDB $$@
	cp $$@ ../utils/
endef

$(foreach target, $(EXECUTABLE), $(eval $(call make-target, $(target))))
$(foreach bdir, $(OBJ_DIR), $(eval $(call make-goal-cxx, $(bdir))))
$(foreach bdir, $(OBJ_DIR), $(eval $(call make-goal-c, $(bdir))))

#$(PWD)/obj/Parser/symbolicator.o:$(PWD)/src/Parser/symbolicator.c
	#$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

#$(OBJ_DIR)%.o:$(SRC_DIR)%.cpp
	#$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -rf obj
	rm -rf bin
