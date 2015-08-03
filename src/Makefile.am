include $(top_srcdir)/am/gcov.am
include $(top_srcdir)/am/gprof.am
include $(top_srcdir)/am/valgrind.am
include $(top_srcdir)/am/cppcheck.am
include $(top_srcdir)/am/version.am

AM_CXXFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(PERF_TESTING_CFLAGS) \
	$(MORE_CXXFLAGS)

AM_LDFLAGS = \
	$(PTHREAD_LIBS) \
	$(BACKTRACE_LDFLAGS) \
	$(PERF_TESTING_LDFLAGS) \
	$(CODE_COVERAGE_LDFLAGS)

AM_DEFAULT_SOURCE_EXT = .cc

CODE_COVERAGE_IGNORE_PATTERN += "test_*.cc" "datum.hh"

#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed

REPO_VERSION = $(top_srcdir)/bin/repo-version

bin_PROGRAMS = \
	factory_time

lib_LIBRARIES = \
	libfactory.a

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
	test_kernel \
	test_discovery \
	test_poller

check_PROGRAMS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_websocket \
	test_shmem \
	test_timer \
	test_uint128 \
	test_kernel \
	test_discovery \
	test_poller


# disable stack trace for tests where a child process must fail
#test_socket_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_BACKTRACE
test_socket_LDADD        = -lfactory -L.
test_offline_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_BACKTRACE
test_websocket_CPPFLAGS  = $(AM_CPPFLAGS) -DFACTORY_NO_BACKTRACE
test_discovery_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_NO_CPU_BINDING
