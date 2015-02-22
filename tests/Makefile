BIN       = test_buffer
CC        = g++
CXXFLAGS  = -gdwarf-3 -O2 -std=c++11 -Wall -Wextra
LDFLAGS   = -lssl -lcrypto -lresolv
INCLUDE   = -I../src


#all: test_buffer test_marshaling test_socket test_discovery
all: test_discovery

test_buffer: test_buffer.cc test_buffer.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_buffer.cc $(LDFLAGS) -o test_buffer

test_marshaling: test_marshaling.cc test_marshaling.hh datum.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_marshaling.cc $(LDFLAGS) -o test_marshaling

test_socket: test_socket.cc datum.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_socket.cc $(LDFLAGS) -o test_socket

test_discovery: test_discovery.cc test_discovery.hh ../src/factory/*.hh
	$(CC) $(CXXFLAGS) $(INCLUDE) test_discovery.cc $(LDFLAGS) -o test_discovery

#run: test_buffer test_marshaling test_socket
run: test_discovery
#	./test_buffer
#	./test_marshaling
#	./test_socket
	sudo ./cluster --cluster cat --nodes 4 --user $(USER) -- $(PWD)/test_discovery

clean:
	rm -f test_buffer
	rm -f test_marshaling
	rm -f test_socket
	rm -f test_discovery
