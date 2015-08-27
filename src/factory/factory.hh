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
	#include <sstream>
	#include <vector>
	#include <algorithm>
	#include <iterator>
	#include <chrono>
	#include <string>
	#include <functional>
	
	// Standard portable types
	#include <cinttypes>
	#include <cstddef>
	
	// C++ multi-threading
	#include <mutex>
	#include <condition_variable>
	#include <atomic>
	
	// Base system
	#include "error.hh"
	#include "type.hh"
	#include "kernel.hh"
	#include "managed_object.hh"
	#include "basic_factory.hh"
	
#endif

#endif // FACTORY_FACTORY_BASE_HH
