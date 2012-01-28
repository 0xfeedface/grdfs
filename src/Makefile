SYS=$(shell uname)
MV=mv
RM=rm -f

CXXFLAGS=-W -Wall -O2 -std=c++0x
CXXSOURCES=Dictionary.cc NativeReasoner.cc OpenCLReasoner.cc Reasoner.cc Store.cc
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

all: move

move: main
	$(MV) $< $(BUILD)/grdfs

main: $(CXXOBJECTS)
	$(CXX) -o $@ main.cc $(CXXOBJECTS) $(CXXFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	-$(RM) *.o
	-$(RM) $(BUILD)/grdfs

