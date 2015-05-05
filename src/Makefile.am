#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed

AM_DEFAULT_SOURCE_EXT = .cc

TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_spinlock \
	test_endpoint \
	test_encoding

check_PROGRAMS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_discovery \
	test_spinlock \
	test_endpoint \
	test_encoding \
	test_websocket

# disable stack trace for tests where a child process must fail
test_offline_CPPFLAGS    = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE
test_websocket_CPPFLAGS  = $(AM_CPPFLAGS) -DFACTORY_NO_STACK_TRACE

# disable Factory global objects where they are not needed to reduce build time
test_buffer_CPPFLAGS     = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_spinlock_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_endpoint_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_encoding_CPPFLAGS   = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
test_marshaling_CPPFLAGS = $(AM_CPPFLAGS) -DFACTORY_NO_DERIVED_SYSTEM
