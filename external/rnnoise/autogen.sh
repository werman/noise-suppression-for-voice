#!/bin/sh
# Run this to set up the build system: configure, makefiles, etc.
set -e

srcdir=`dirname $0`
test -n "$srcdir" && cd "$srcdir"

./download_model.sh

echo "Updating build configuration files for rnnoise, please wait...."

autoreconf -isf
