SYS=$(shell uname)
MV=mv
RM=rm -f

MAIN=main.cc

CXXFLAGS=-g -Wall -O3 -std=c++0x
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
INCLUDE=-I/usr/include/raptor2 -I/usr/local/include/raptor2
CXXFLAGS+=$(INCLUDE)
LDFLAGS=-lraptor2

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

.PHONY: clean
clean:
	-$(RM) *.o
	-$(RM) $(BUILD)/grdfs

