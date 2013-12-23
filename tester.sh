#!/bin/bash
for i in {0..9}
do
	echo "
		Launch $i"
	if [ $1 == 'dc' ]; then
		./cachesim/cachesim --dump-memory --statistics --disable-cache tests/$1.cfg < tests/10000trace$i > tests/results/$1/10000$i.txt
	else
		./cachesim/cachesim --dump-memory --statistics tests/$1.cfg < tests/10000trace$i > tests/results/$1/10000$i.txt
	fi
	diff tests/results/$1/10000$i.txt tests/answers/$1/10000$i.txt
done
