#!/bin/sh

cmd=$1
speech=$2
noise=$3
output=$4
count=$5
split=$6
#echo seq $split parallel -j +2 "$cmd $speech $noise $output.{} $count"
seq $split | parallel -j +2 "$cmd $speech $noise $output.{} $count"
mv $output.1 $output
for i in $output.*
do
	cat $i >> $output
	rm $i
done
