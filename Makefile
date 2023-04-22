all: main

CXX = clang++
override CXXFLAGS += -g -Wno-everything -std=c++17

SRCS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.cpp' -print | sed -e 's/ /\\ /g')
HEADERS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.h' -print)

LDFLAGS = -L /nix/store/1wgp3wjc61hhg5z2af5s7hcab76sz8zm-systemc-2.3.3/lib-linux64

PROCESSOR_SRC = processor/processor_run.cpp

main: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PROCESSOR_SRC) -o "$@" -lsystemc -lm $(LDFLAGS)

main-debug: $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -O0 $(PROCESSOR_SRC) -o "$@" -lsystemc -lm $(LDFLAGS)

clean:
	rm -f main main-debug