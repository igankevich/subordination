include $(top_srcdir)/am/gcov.am
include $(top_srcdir)/am/gprof.am
include $(top_srcdir)/am/valgrind.am
include $(top_srcdir)/am/cppcheck.am

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

AM_CTAGSFLAGS = --extra=+f -R .

AM_DEFAULT_SOURCE_EXT = .cc

CODE_COVERAGE_IGNORE_PATTERN += "test_*.cc" "datum.hh"

#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed

ONE_FILE_TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_websocket \
	test_shmem \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_timer \
	test_uint128 \
	test_uint128_custom \
	test_offline \
	test_poller

check_PROGRAMS = \
	$(ONE_FILE_TESTS) \
	test_appserver_app \
	test_appserver_srv \
	test_posix

TESTS = \
	$(ONE_FILE_TESTS) \
	test_appserver_srv \
	test_static

test_uint128_custom_SOURCES = test_uint128.cc
test_uint128_custom_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_FORCE_CUSTOM_UINT128

test_websocket_SOURCES = test_socket.cc
test_websocket_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_TEST_WEBSOCKET

test_offline_SOURCES = test_socket.cc
test_offline_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_TEST_OFFLINE

test_appserver_srv_SOURCES = test_appserver.cc
test_appserver_srv_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_TEST_SERVER \
	-DFACTORY_APP_PATH=$(abs_srcdir)/test_appserver_app

test_appserver_app_SOURCES = test_appserver.cc
test_appserver_app_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_TEST_APP

test_discovery_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_NO_CPU_BINDING
