libpthread = find_library('pthread')
librt = find_library('rt')
threads = [libpthread, librt]

test('buffer', executable('buffer-test', sources: 'test_buffer.cc'))
test('socket', executable('socket-test', sources: 'test_socket.cc', dependencies: threads))
test('offline', executable('offline-test', sources: 'test_socket.cc', dependencies: threads, cpp_args: '-DFACTORY_TEST_OFFLINE'))
test('shmem', executable('shmem-test', sources: 'test_shmem.cc', dependencies: threads))
test('locks', executable('locks-test', sources: 'test_locks.cc', dependencies: threads))
test('endpoint', executable('endpoint-test', sources: 'test_endpoint.cc'))
test('encoding', executable('encoding-test', sources: 'test_encoding.cc'))
test('timer', executable('timer-test', sources: 'test_timer.cc', dependencies: threads))
test('uint128-builtin', executable('uint128-test', sources: 'test_uint128.cc'))
test('uint128-emulate', executable('uint128-emulate-test', sources: 'test_uint128.cc', cpp_args: '-DFACTORY_FORCE_CUSTOM_UINT128'))
test('poller', executable('poller-test', sources: 'test_poller.cc'))

app_exe = executable('app-test', sources: 'test_appserver.cc', dependencies: threads, cpp_args: '-DFACTORY_TEST_APP')
srv_exe = executable('srv-test', sources: 'test_appserver.cc', dependencies: threads, cpp_args: ['-DFACTORY_TEST_SERVER', '-DFACTORY_APP_PATH=./src/app-test'])
test('app', srv_exe)
