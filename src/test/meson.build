test(
	'buffer',
	executable(
		'buffer-test',
		sources: 'buffer_test.cc',
		include_directories: srcdir
	)
)

test(
	'kernel-stream',
	executable(
		'kernel-stream-test',
		sources: 'kernel_stream_test.cc',
		include_directories: srcdir
	)
)

test(
	'nic-server',
	executable(
		'nic-server-test',
		sources: 'nic_server_test.cc',
		dependencies: threads,
		include_directories: srcdir,
		link_args: static_link_flags
	)
)

test(
	'nic-server-offline',
	executable(
		'nic-server-offline-test',
		sources: 'nic_server_test.cc',
		dependencies: threads,
		cpp_args: '-DFACTORY_TEST_OFFLINE',
		include_directories: srcdir
	)
)

test(
	'shmem',
	executable(
		'shmem-test',
		sources: 'shmem_test.cc',
		dependencies: threads,
		include_directories: srcdir
	)
)

test(
	'locks',
	executable(
		'locks-test',
		sources: 'locks_test.cc',
		dependencies: threads,
		include_directories: srcdir
	)
)

test(
	'sysx::endpoint',
	executable(
		'endpoint-test',
		sources: 'endpoint_test.cc',
		include_directories: srcdir
	)
)

test(
	'timer-server',
	executable(
		'timer-server-test',
		sources: 'timer_server_test.cc',
		dependencies: threads,
		include_directories: srcdir
	)
)

test(
	'std::uint128 (builtin)',
	executable(
		'uint128-test',
		sources: 'uint128_test.cc',
		include_directories: srcdir
	)
)

test(
	'std::uint128 (emulate)',
	executable(
		'uint128-emulate-test',
		sources: 'uint128_test.cc',
		cpp_args: '-DFACTORY_FORCE_CUSTOM_UINT128',
		include_directories: srcdir
	)
)

test(
	'poller',
	executable(
		'poller-test',
		sources: 'poller_test.cc',
		include_directories: srcdir
	)
)


app_exe = executable(
	'process-server-test-app',
	sources: 'process_server_test.cc',
	dependencies: threads,
	include_directories: srcdir,
	cpp_args: '-DFACTORY_TEST_APP'
)

srv_exe = executable(
	'process-server-test',
	sources: 'process_server_test.cc',
	dependencies: threads,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_TEST_SERVER', '-DFACTORY_APP_PATH=./src/test/process-server-test-app']
)

test('process-server', srv_exe)

executable(
	'posix-compliance',
	sources: 'posix_test.cc',
	include_directories: srcdir
)
