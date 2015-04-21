#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>
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

// C++ multi-threading.
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

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

// Cryptography.
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/md5.h> /* md5 hash */
#include <openssl/sha.h> /* sha1 hash */


// Base system
#include "base.hh"
#include "error.hh"
#include "network.hh"
#include "base64.hh"
#include "endpoint.hh"
#include "socket.hh"
#include "buffer.hh"
//#include "topology.hh"
#include "endpoint.hh"
#include "poller.hh"
#include "type.hh"
//#include "resource.hh"
#include "kernel.hh"
#include "repository.hh"
#include "server.hh"
#include "socket_server.hh"
#include "discovery.hh"
#include "strategy.hh"

// Configuration
#include "release_configuration.hh"

// Derived system
#include "derived.hh"
#include "kernel.hh"
