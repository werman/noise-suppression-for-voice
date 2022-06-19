LIBS=-ldl -lrt

FST_CPPFLAGS=-I../../fst -I.. -DFST_DONT_DEPRECATE_UNKNOWN=1
FST_CXXFLAGS=-g -O0

archs: FstHost.32 FstHost.64

FstHost.32: ARCHFLAGS=-m32
FstHost.64: ARCHFLAGS=-m64

FstHost FstHost.32 FstHost.64: FstHost.cpp ../../fst/fst.h ../fst_utils.h
	$(CXX) $(FST_CPPFLAGS) $(CPPFLAGS) $(FST_CXXFLAGS) $(CXXFLAGS) $(ARCHFLAGS) $< -o $@ $(LIBS)

.PHONY: clean archs default
clean:
	rm -f FstHost FstHost.32 FstHost.64
