pkill test_discovery
pkill test_discovery
pkill test_discovery
sleep 1
./test_discovery 127.0.0.1:60000 60000 60002 &
sleep 1
./test_discovery 127.0.0.1:60001 60000 60002 &
wait
