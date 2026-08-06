DUMP ?= 0
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -O0 -g -I.. -DENABLE_DUMP=$(DUMP)
LDFLAGS  =

TARGET   = pb_vm

# main.cpp is in the current directory; all other sources are in ..
SOURCES  = main.cpp ../compiler.cpp ../vm.cpp ../program.cpp 
HEADERS  = ../compiler.h ../program.h ../ffi.h ../pixelblaze.h ../runtime.h ../vm.h 

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

# 编译时启用 ENABLE_DUMP（宏值为 2，开启详细输出）
dump: clean
	$(MAKE) DUMP=1

test: $(TARGET)
	./testall.sh

clean:
	rm -f $(TARGET)
	rm -rf $(TARGET).dSYM

clearjson:
	rm -f *_pixelCount*_frameCount*.json

clear: clean clearjson	

.PHONY: all dump test clean clearjson clear