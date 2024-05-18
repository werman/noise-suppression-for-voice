## GNU makefile for rnnoise documentation.

-include ../package_version

all: doxygen

doxygen: Doxyfile ../include/rnnoise.h
	doxygen

pdf: doxygen
	make -C latex

clean:
	$(RM) -r html
	$(RM) -r latex

distclean: clean
	$(RM) Doxyfile

.PHONY: all clean distclean doxygen pdf

../package_version:
	@if [ -x ../update_version ]; then \
		../update_version || true; \
	elif [ ! -e $@ ]; then \
		echo 'PACKAGE_VERSION="unknown"' > $@; \
	fi

# run autoconf-like replacements to finalize our config
Doxyfile: Doxyfile.in Makefile ../package_version
	sed -e 's/@PACKAGE_NAME@/rnnoise/' \
	    -e 's/@PACKAGE_VERSION@/$(PACKAGE_VERSION)/' \
	    -e 's/@top_srcdir@/../' \
	  < $< > $@
