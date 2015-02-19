BIN       = test_buffer
CC        = g++
CXXFLAGS  = -gdwarf-3 -O2 -std=c++11 -Wall -Wextra
LDFLAGS   = -lssl -lcrypto -lresolv
INCLUDE   = -I../src


all: test_buffer test_marshaling test_socket test_discovery

test_buffer: test_buffer.cc test_buffer.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_buffer.cc $(LDFLAGS) -o test_buffer

test_marshaling: test_marshaling.cc test_marshaling.hh datum.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_marshaling.cc $(LDFLAGS) -o test_marshaling

test_socket: test_socket.cc datum.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_socket.cc $(LDFLAGS) -o test_socket

test_discovery: test_discovery.cc datum.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_discovery.cc $(LDFLAGS) -llxc -o test_discovery

run: test_buffer test_marshaling test_socket
	./test_buffer
	./test_marshaling
#	./test_socket

clean:
	rm -f test_buffer
	rm -f test_marshaling
	rm -f test_socket
	rm -f test_discovery
