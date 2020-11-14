#!/bin/bash

cd build
rm -f /tmp/transactions  # удаляем состояние
pkill sbnd || true       # убиваем старый процесс
# запускаем планировщик с конфигурацией
cat > sbnd.properties << EOF
network.allowed-interface-addresses=1.2.3.4/24
transactions.directory=/tmp
EOF
./src/subordination/daemon/sbnd config=$PWD/sbnd.properties &
sleep 1
# запускаем программу
./src/subordination/daemon/sbnc ./src/examples/hello-world/hello-world
pkill sbnd # убиваем процесс планировщика
wait       # ждем завершения 
