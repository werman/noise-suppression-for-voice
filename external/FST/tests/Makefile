#####################################################################
## GNUmakefile for GNU Makefiles ('make')
##
## see makefile for Windows' 'nmake'
#####################################################################

CPPFLAGS=-I..

sources=c_test.c cxx_test.cpp
programs=$(patsubst %.c,%,$(filter %.c,$(sources))) $(patsubst %.cpp,%,$(filter %.cpp,$(sources)))

.PHONY: all clean preproc

all: $(programs)

clean:
	-rm $(programs)
	-rm *.i *.ii

preproc: $(patsubst %.c,%.i,$(filter %.c,$(sources))) $(patsubst %.cpp,%.ii,$(filter %.cpp,$(sources)))


%.i: %.c
	$(CPP) $(CPPFLAGS) $< > $@
%.ii: %.cpp
	$(CPP) $(CPPFLAGS) $< > $@
