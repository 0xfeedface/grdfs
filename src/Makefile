SYS=$(shell uname)
MV=mv
RM=rm -rf

MAIN=main.cc

CXXFLAGS=-g -Wall -O3 -std=c++11
CXXSOURCES=$(filter-out $(MAIN),$(wildcard *.cc))
CXXOBJECTS=$(CXXSOURCES:.cc=.o)

# compiler
ifeq ($(SYS), Darwin)
CXX=clang++
CXXFLAGS+=-stdlib=libc++
else
CXX=g++
endif

BUILD=../build
INCLUDE=../include

ifeq ($(SYS), Darwin)
CXXFLAGS+=-I../include
LDFLAGS+=-framework OpenCL
else
CXXFLAGS+=-I$(AMDAPPSDKROOT)/include
LDFLAGS+=-L$(AMDAPPSDKROOT)/lib -laticalcl
endif

all: $(BUILD)/grdfs

$(BUILD)/grdfs: $(CXXOBJECTS) $(MAIN)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(MAIN) $(CXXOBJECTS)

Reasoner.o: Store.o

.PHONY: clean
clean:
	-$(RM) *.o
	-$(RM) $(BUILD)/grdfs
	-$(RM) $(BUILD)/grdfs.dSYM
