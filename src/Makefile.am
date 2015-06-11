#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed

AM_DEFAULT_SOURCE_EXT = .cc
AM_CXXFLAGS = $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

bin_PROGRAMS = \
	factory_time

TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_timer \
	test_swap

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
	test_swap

CODE_COVERAGE_LCOV_OPTIONS = --checksum --rc lcov_branch_coverage=1
CODE_COVERAGE_IGNORE_PATTERN = '/usr/include/*'
CODE_COVERAGE_GENHTML_OPTIONS = --demangle-cpp --css-file=../util/gcov.css
@CODE_COVERAGE_RULES@

# disable stack trace for tests where a child process must fail
test_offline_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE
test_websocket_CPPFLAGS  = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE

# disable Factory global objects, where they are not needed, to reduce build time
test_buffer_CPPFLAGS     = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_spinlock_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_endpoint_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_encoding_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_marshaling_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_swap_CPPFLAGS       = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
factory_time_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
#test_shmem_CPPFLAGS      = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM

test_discovery_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_DISABLE_CPU_BINDING

include $(top_srcdir)/am/valgrind.am
