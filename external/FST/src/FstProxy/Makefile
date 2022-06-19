FST_CPPFLAGS=-I../../fst -I.. -DFST_DONT_DEPRECATE_UNKNOWN=1
FST_CXXFLAGS=-g -O0 -fPIC

LIBS=

archs: FstProxy.32.so FstProxy.64.so

FstProxy.32.so: ARCHFLAGS=-m32
FstProxy.64.so: ARCHFLAGS=-m64

FstProxy.so FstProxy.32.so FstProxy.64.so: FstProxy.cpp ../../fst/fst.h ../fst_utils.h
	$(CXX) -shared $(FST_CPPFLAGS) $(CPPFLAGS) $(FST_CXXFLAGS) $(CXXFLAGS) $(ARCHFLAGS) $< -o $@ $(LIBS)

.PHONY: clean archs default
clean:
	rm -f *.so
