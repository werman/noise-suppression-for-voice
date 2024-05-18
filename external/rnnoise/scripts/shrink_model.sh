#!/bin/sh

for i in rnnoise_data.c
do
    cat src/$i | perl -ne 'if (/DEBUG/ || /#else/) {$skip=1} if (!$skip && !/ifdef DOT_PROD/) {s/^ *//; s/, /,/g; print $_} elsif (/endif/) {$skip=0}' > tmp_data.c
    mv tmp_data.c src/$i
done
