#!/bin/sh

cmd=$1
speech=$2
noise=$3
fgnoise=$4
output=$5
count=$6
rir=$7
split=400
seq $split | parallel -j +2 "$cmd -rir_list $rir $speech $noise $fgnoise $output.{} $count"
mv $output.1 $output
for i in $output.*
do
	cat $i >> $output
	rm $i
done
