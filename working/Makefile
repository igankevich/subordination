tcp: tcp.cc
	g++ -std=c++11 tcp.cc -o tcp

raw: raw.cc
	g++ -std=c++11 raw.cc -o raw

udp: udp.cc
	g++ -std=c++11 udp.cc -o udp

run: 
	./tcp
#	ssh root@ant.apmath.spbu.ru rm ./udp
#	scp udp root@ant.apmath.spbu.ru:
#	ssh root@ant.apmath.spbu.ru ./udp 93.158.134.11:40000
#	scp raw root@ant.apmath.spbu.ru:
#	ssh root@ant.apmath.spbu.ru ./raw 93.158.134.11
#	sudo ./raw 8.8.8.8
#	./udp 8.8.8.8:40000
#	./udp 192.168.1.20:50000

clean:
	rm -f tcp raw udp
