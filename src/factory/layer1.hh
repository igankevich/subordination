#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <system_error>
#include <queue>
#include <algorithm>
#include <iterator>
#include <chrono>

// Metadata.
#include <string>
#include <functional>
#include <unordered_map>
#include <map>

// Standard portable types.
#include <cinttypes>
#include <cstring>

// C++ threads.
#include <thread>
#include <mutex>
#include <condition_variable>

// Linux (and hopefully POSIX) system
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

// Base system
#include "error.hh"
#include "network.hh"
#include "endpoint.hh"
#include "socket.hh"
#include "buffer.hh"
#include "datastream.hh"
#include "sockets.hh"
#include "event_poller.hh"
#include "base.hh"
#include "type.hh"
#include "resource.hh"
#include "kernel.hh"
#include "websocket.hh"
#include "repository.hh"
#include "server.hh"
#include "socket_server.hh"
#include "servers/web_socket_server.hh"
#include "discovery.hh"
#include "discovery_server.hh"
#include "strategy.hh"
#include "tetris.hh"
