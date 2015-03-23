#!/bin/sh

while [ -n "$1" ]; do
	case $1 in
		--cache)
			cache=1
			shift
			;;
	esac
done


for NUM_PEERS in $(seq 2 50); do
	export NUM_PEERS
	rm -f /tmp/*.cache
	if [ "$cache" = "1" ]; then
		WRITE_CACHE=1 ./test_discovery >/dev/null 2>&1
	fi
	echo -n $NUM_PEERS,
	/usr/bin/time -f 'Elapsed time = %e' ./test_discovery 2>&1 | grep Elapsed
	sleep 1
done
