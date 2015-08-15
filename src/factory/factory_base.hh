#ifndef FACTORY_FACTORY_BASE_HH
#define FACTORY_FACTORY_BASE_HH

// autoconf defines
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif
#include "version.hh"

// Patching stdc++ for Valgrind.
#ifdef HAVE_VALGRIND
#include <valgrind/drd.h>
#include <valgrind/helgrind.h>
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(A) ANNOTATE_HAPPENS_BEFORE(A)
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(A)  ANNOTATE_HAPPENS_AFTER(A)
#endif

#if __cplusplus < 201103L
	#error Factory requires C++11 compiler.
#else
	// STL
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
	#include <string>
	#include <functional>
	#include <unordered_map>
	#include <map>
	#include <set>
	#include <bitset>
	
	// Standard portable types
	#include <cinttypes>
	#include <cstring>
	#include <cstdlib>
	#include <csignal>
	#include <cstddef>
	
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
	#include <sys/ipc.h>
	#include <sys/shm.h>
	#include <sys/wait.h>
	#include <semaphore.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <poll.h>
	#include <ifaddrs.h>
	#ifdef HAVE_EXECINFO_H
		#include <execinfo.h>
	#endif
	
	// Base system
	#include "forward.hh"
	#include "base.hh"
	#include "stdx.hh"
	#include "packstream.hh"
	#include "unistdx.hh"
	#include "error.hh"
	#include "ext.hh"
	#include "type.hh"
	#include "kernel.hh"
	#include "server.hh"
	#include "socket_server.hh"
	#include "app_server.hh"
	#include "discovery.hh"
	#include "strategy.hh"
	
	// Configuration
	#include "release_configuration.hh"
#endif
#endif // FACTORY_FACTORY_BASE_HH
