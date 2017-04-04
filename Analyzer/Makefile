TARGET		:= analyzer
#trace_voucher
#analyzer
#diff
CXX			:= clang++
CC			:= clang
CXXFLAGS	:= -c -Wall -std=c++11 -stdlib=libc++ 
CFLAGS		:= -c -Wall
LDFLAGS		:= -Xlinker /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/LLDB -L/usr/local/Cellar/boost/1.63.0/lib -lboost_regex-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_system-mt
INC_LLDB	:= /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/Headers/
INC_BOOST	:= /usr/local/Cellar/boost/1.63.0/include/
INC_DIR		:= $(PWD)/include/
INC_EVENTS	:= $(PWD)/include/TracingEvents
INC_CLUSTER	:= $(PWD)/include/Cluster
INCLUDES	:= -I $(INC_LLDB) -I $(INC_BOOST) -I$(INC_DIR) -I$(INC_EVENTS) -I$(INC_CLUSTER)
MODULES		:= TracingEvents Loader Parser Cluster Canonization
SRC_DIR		:= $(addprefix $(PWD)/src/, $(MODULES))
OBJ_DIR		:= $(PWD)/build $(addprefix $(PWD)/build/, $(MODULES))
BIN_DIR		:= $(PWD)/bin/
SOURCES 	:= $(PWD)/src/$(TARGET).cpp $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.cpp))
C_SOURCES 	:= $(foreach sdir, $(SRC_DIR), $(wildcard $(sdir)/*.c))
OBJECTS 	:= $(patsubst $(PWD)/src/%.cpp, $(PWD)/build/%.o, $(SOURCES))
C_OBJECTS 	:= $(patsubst $(PWD)/src/%.c, $(PWD)/build/%.o, $(C_SOURCES))
EXECUTABLE	:= $(BIN_DIR)$(TARGET)

vpath %.cpp $(SRC_DIR) $(PWD)/src/

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
	install_name_tool -change @rpath/LLDB.framework/LLDB /Applications/Xcode.app/Contents/SharedFrameworks/LLDB.framework/LLDB $(EXECUTABLE)

directories: $(OBJ_DIR) $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@
    
$(EXECUTABLE): $(OBJECTS) $(C_OBJECTS) 
	$(CXX) $(INCLUDES) $(OBJECTS) $(C_OBJECTS) $(LDFLAGS) -o $@ 

#$(CC) $(LDFLAGS) $(INCLUDES) $(OBJECTS) -o $@
$(foreach bdir, $(OBJ_DIR), $(eval $(call make-goal-cxx, $(bdir))))

#$(foreach bdir, $(OBJ_DIR), $(eval $(call make-goal-c, $(bdir))))

$(PWD)/build/Parser/mysymbolicator.o:$(PWD)/src/Parser/mysymbolicator.c
	$(CC) $(INCLUDES) $(CFLAGS) $< -o $@

#$(OBJ_DIR)%.o:$(SRC_DIR)%.cpp
#$(CC) $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -rf build
	rm -rf bin
