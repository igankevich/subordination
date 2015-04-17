#-Wstack-protector -fstack-protector -- security concerns
#-Wpadded                            -- detects padding
#-fmudflapth                         -- pointer range check
#-Winline                            -- report when inline fails
#-Wunsafe-loop-optimizations         -- false positive for Process:execute
#-Waggregate-return                  -- detects return of structs and unions
#-Wabi                               -- probably not needed


TESTS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_spinlock \
	test_endpoint \
	test_base64

check_PROGRAMS = \
	test_buffer \
	test_marshaling \
	test_socket \
	test_offline \
	test_discovery \
	test_spinlock \
	test_endpoint \
	test_base64 \
	test_websocket

test_buffer_SOURCES     = test_buffer.cc
test_marshaling_SOURCES = test_marshaling.cc
test_socket_SOURCES     = test_socket.cc
test_offline_SOURCES    = test_offline.cc
test_discovery_SOURCES  = test_discovery.cc
test_spinlock_SOURCES   = test_spinlock.cc
test_endpoint_SOURCES   = test_endpoint.cc
test_base64_SOURCES     = test_base64.cc
test_websocket_SOURCES  = test_websocket.cc
