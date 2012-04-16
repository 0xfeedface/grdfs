SYS=$(shell uname)
MV=mv
RM=rm -rf

MAIN=main.cc

CXXFLAGS=-g -Wall -O3
CXXSOURCES=$(filter-out $(MAIN),$(wildcard *.cc))
CXXOBJECTS=$(CXXSOURCES:.cc=.o)
CXXHEADERS=$(wildcard *.h)

# compiler
ifeq ($(SYS), Darwin)
CXX=clang++
CXXFLAGS+=-std=c++11 -stdlib=libc++
else
CXX=/opt/llvm-3.0/bin/clang++
CXXFLAGS+=-std=c++0x
endif

BUILD=../build
INCLUDE=../include

ifeq ($(SYS), Darwin)
CXXFLAGS+=-I../include
LDFLAGS+=-framework OpenCL
else
CXXFLAGS+=-I../include -I$(AMDAPPSDKROOT)/include
LDFLAGS+=-L$(AMDAPPSDKROOT)/lib -lOpenCL
endif

all: $(BUILD)/grdfs

$(BUILD)/grdfs: $(CXXOBJECTS) $(CXXHEADERS) $(MAIN)
	mkdir -p $(BUILD) && $(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(MAIN) $(CXXOBJECTS)

Reasoner.o: Store.o

.PHONY: clean
clean:
	-$(RM) *.o
	-$(RM) $(BUILD)/grdfs
	-$(RM) $(BUILD)/grdfs.dSYM
