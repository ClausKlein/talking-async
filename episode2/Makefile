CXX?=g++
CXXFLAGS:=-std=c++20 -Wall -Wextra -Wpedantic -fno-inline -g -fsanitize=address
CPPFLAGS:=-I$(ASIO_ROOT)/include -DASIO_ENABLE_HANDLER_TRACKING #TODO(CK): -DASIO_NO_DEPRECATED
SOURCE:=$(wildcard *.cpp)
PROGRAMS:=$(SOURCE:.cpp=)
DSYM:=$(SOURCE:.cpp=.dSYM)

all: $(PROGRAMS)

clean:
	rm -rf $(PROGRAMS) $(DSYM) *~
