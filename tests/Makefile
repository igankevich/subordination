BIN       = ../bin
CC        = g++
CXXFLAGS  = -gdwarf-3 -O2 -std=c++11 -Wall -Wextra
LDFLAGS   = -lssl -lcrypto -lresolv
INCLUDE   = -I../src


all: test_buffer
all: test_marshaling
all: test_socket
all: test_discovery

test_buffer: test_buffer.cc test_buffer.hh ../src/factory/*.hh $(BIN)
	$(CC) $(CXXFLAGS) $(INCLUDE) test_buffer.cc $(LDFLAGS) -o $(BIN)/test_buffer

test_marshaling: test_marshaling.cc test_marshaling.hh datum.hh ../src/factory/*.hh $(BIN)
	$(CC) $(CXXFLAGS) $(INCLUDE) test_marshaling.cc $(LDFLAGS) -o $(BIN)/test_marshaling

test_socket: test_socket.cc datum.hh ../src/factory/*.hh $(BIN)
	$(CC) $(CXXFLAGS) $(INCLUDE) test_socket.cc $(LDFLAGS) -o $(BIN)/test_socket

test_discovery: test_discovery.cc test_discovery.hh ../src/factory/*.hh $(BIN)
	$(CC) $(CXXFLAGS) $(INCLUDE) test_discovery.cc $(LDFLAGS) -o $(BIN)/test_discovery

$(BIN):
	mkdir -p $(BIN)

#run: test_buffer test_marshaling test_socket
run: test_discovery
#	$(BIN)/test_buffer
#	$(BIN)/test_marshaling
#	$(BIN)/test_socket
	sudo ./cluster --cluster cat --nodes 4 --user $(USER) -- $(BIN)/test_discovery

clean:
	rm -f $(BIN)/test_*
