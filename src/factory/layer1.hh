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
#include <tuple>
#include <array>

// Metadata.
#include <string>
#include <functional>
#include <unordered_map>
#include <map>
#include <set>

// Standard portable types.
#include <cinttypes>
#include <cstring>
#include <cstdlib>

// C++ threads.
#include <thread>
#include <mutex>
#include <condition_variable>

// Linux (and hopefully POSIX) system
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <csignal>
#include <ifaddrs.h>


// Base system
#include "base.hh"
#include "error.hh"
#include "network.hh"
#include "endpoint.hh"
#include "socket.hh"
#include "buffer.hh"
#include "datastream.hh"
//#include "topology.hh"
#include "endpoint.hh"
#include "sockets.hh"
#include "poller.hh"
#include "type.hh"
//#include "resource.hh"
#include "kernel.hh"
#include "base64.hh"
//#include "websocket.hh"
#include "repository.hh"
#include "server.hh"
#include "socket_server.hh"
//#include "servers/web_socket_server.hh"
#include "discovery.hh"
#include "strategy.hh"
#include "tetris.hh"
