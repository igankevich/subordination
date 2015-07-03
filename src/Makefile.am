include $(top_srcdir)/am/gcov.am
include $(top_srcdir)/am/valgrind.am
include $(top_srcdir)/am/version.am

AM_CXXFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(MORE_CXXFLAGS)

AM_LDFLAGS = \
	$(PTHREAD_LIBS) \
	$(CODE_COVERAGE_LDFLAGS)

CODE_COVERAGE_IGNORE_PATTERN += "test_*.cc" "datum.hh"

#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed

REPO_VERSION = $(top_srcdir)/bin/repo-version
AM_DEFAULT_SOURCE_EXT = .cc

bin_PROGRAMS = \
	factory_time

TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_websocket \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_timer \
	test_uint128 \
	test_kernel

check_PROGRAMS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_discovery \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_websocket \
	test_shmem \
	test_timer \
	test_uint128 \
	test_kernel


# disable stack trace for tests where a child process must fail
test_offline_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE
test_websocket_CPPFLAGS  = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE

# disable Factory global objects, where they are not needed, to reduce build time
test_buffer_CPPFLAGS     = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_spinlock_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_endpoint_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_encoding_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_marshaling_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
factory_time_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
#test_shmem_CPPFLAGS      = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_uint128_CPPFLAGS     = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM

test_discovery_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_DISABLE_CPU_BINDING
