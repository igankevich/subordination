#if __cplusplus < 2011L
	#error Factory requires C++11 compiler.
#else

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
	#include <random>
	
	// Metadata
	#include <string>
	#include <functional>
	#include <unordered_map>
	#include <map>
	#include <set>
	
	// Standard portable types
	#include <cinttypes>
	#include <cstring>
	#include <cstdlib>
	
	// C++ multi-threading
	#include <thread>
	#include <mutex>
	#include <condition_variable>
	#include <atomic>
	
	// POSIX system
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <poll.h>
	#include <csignal>
	#include <ifaddrs.h>
	#include <execinfo.h>
	
	// Base system
	#include "base.hh"
	#include "error.hh"
	#include "network.hh"
	#include "encoding.hh"
	#include "endpoint.hh"
	#include "buffer.hh"
	#include "socket.hh"
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
	
	#if !defined(FACTORY_NO_DERIVED_SYSTEM)
		// Derived system
		#include "derived.hh"
		#include "kernel.hh"
	#elif defined(FACTORY_EXTEND_DERIVED_SYSTEM)
		#include "kernel.hh"
	#endif

#endif
