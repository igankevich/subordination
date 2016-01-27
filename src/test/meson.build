test('buffer', executable('buffer-test', sources: 'buffer_test.cc', include_directories: srcdir))
test('socket', executable('socket-test', sources: 'socket_test.cc', dependencies: threads, include_directories: srcdir))
test('offline', executable('offline-test', sources: 'socket_test.cc', dependencies: threads, cpp_args: '-DFACTORY_TEST_OFFLINE', include_directories: srcdir))
test('shmem', executable('shmem-test', sources: 'shmem_test.cc', dependencies: threads, include_directories: srcdir))
test('locks', executable('locks-test', sources: 'locks_test.cc', dependencies: threads, include_directories: srcdir))
test('endpoint', executable('endpoint-test', sources: 'endpoint_test.cc', include_directories: srcdir))
test('timer', executable('timer-test', sources: 'timer_test.cc', dependencies: threads, include_directories: srcdir))
test('uint128-builtin', executable('uint128-test', sources: 'uint128_test.cc', include_directories: srcdir))
test('uint128-emulate', executable('uint128-emulate-test', sources: 'uint128_test.cc', cpp_args: '-DFACTORY_FORCE_CUSTOM_UINT128', include_directories: srcdir))
test('poller', executable('poller-test', sources: 'poller_test.cc', include_directories: srcdir))

app_exe = executable(
	'app-test',
	sources: 'appserver_test.cc',
	dependencies: threads,
	include_directories: srcdir,
	cpp_args: '-DFACTORY_TEST_APP'
)
srv_exe = executable(
	'srv-test',
	sources: 'appserver_test.cc',
	dependencies: threads,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_TEST_SERVER', '-DFACTORY_APP_PATH=./src/test/app-test']
)
test('app', srv_exe)
