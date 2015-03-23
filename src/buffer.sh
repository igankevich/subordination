#!/bin/sh

for SLEEP_TIME in $(seq 0 100); do
	export SLEEP_TIME
	echo -n $SLEEP_TIME,
	./test_socket 2>&1 | sed -rne 's/.*Buffer size = (.*)/\1/p' | sort -rn | head -n1
	sleep 1
done
