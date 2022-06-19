#!/bin/sh

fst_h="$1"

if [ "x${fst_h}" = "x" ]; then
 fst_h=fst/fst.h
fi

version=$( \
grep FST_ "${fst_h}" \
| egrep -v "# *define " \
| grep -v FST_fst_h_ \
| grep -v UNKNOWN \
| grep -c . \
)
unknown=$( \
grep FST_ "${fst_h}" \
| egrep -v "# *define " \
| grep -v FST_fst_h_ \
| egrep "FST_.*_UNKNOWN" \
| grep -c . \
)


sed -e "s|\(# *define  *FST_MINOR_VERSION  *\)[0-9]*$|\1${version}|" -i "${fst_h}"
echo "version: ${version}"
echo "identifiers: known:${version} unknown:${unknown}"
