test(
	'std::uint128 (builtin)',
	executable(
		'uint128-test',
		sources: 'uint128_test.cc',
		include_directories: srcdir,
	)
)

test(
	'std::uint128 (emulate)',
	executable(
		'uint128-emulate-test',
		sources: 'uint128_test.cc',
		cpp_args: '-DFACTORY_FORCE_CUSTOM_UINT128',
		include_directories: srcdir,
	)
)

test(
	'stdx::spin_mutex, sys::sysv_semaphore',
	executable(
		'locks-test',
		sources: 'locks_test.cc',
		dependencies: libpthread,
		include_directories: srcdir,
	)
)

test(
	'sys::shared_mem',
	executable(
		'shmem-test',
		sources: 'shmem_test.cc',
		dependencies: libpthread,
		include_directories: srcdir,
	)
)

test(
	'sys::endpoint',
	executable(
		'endpoint-test',
		sources: 'endpoint_test.cc',
		include_directories: srcdir,
	)
)

test(
	'sys::event_poller',
	executable(
		'poller-test',
		sources: 'poller_test.cc',
		include_directories: srcdir,
	)
)

test(
	'sys::file, sys::direntry, sys::path',
	executable(
		'sys-test',
		sources: 'sys_test.cc',
		include_directories: srcdir,
	)
)

test(
	'sys::fildesbuf',
	executable(
		'buffer-test',
		sources: 'buffer_test.cc',
		include_directories: srcdir,
	)
)

test(
	'factory::Kernel_stream',
	executable(
		'kernel-stream-test',
		sources: 'kernel_stream_test.cc',
		include_directories: srcdir,
	)
)

test_runner = executable(
	'test-runner',
	sources: 'test_runner.cc',
	include_directories: srcdir,
)

executable(
	'nic-server-test',
	sources: 'nic_server_test.cc',
	dependencies: libpthread,
	include_directories: srcdir,
)

executable(
	'nic-server-offline-test',
	sources: 'nic_server_test.cc',
	dependencies: libpthread,
	include_directories: srcdir,
	cpp_args: [ '-DFACTORY_TEST_OFFLINE' ],
)

test(
	'factory::NIC_server (normal)',
	test_runner,
	args: [
		'--strategy', 'master-slave',
		'--exec', './nic-server-test', '--role', 'master',
		'--exec', './nic-server-test', '--role', 'slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::NIC_server (slave failure)',
	test_runner,
	args: [
		'--strategy', 'master-slave',
		'--exec', './nic-server-offline-test', '--role', 'master',
		'--exec', './nic-server-offline-test', '--role', 'slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::Timer_server',
	executable(
		'timer-server-test',
		sources: 'timer_server_test.cc',
		dependencies: libpthread,
		include_directories: srcdir,
	)
)

app_exe = executable(
	'process-server-test-app',
	sources: 'process_server_test.cc',
	dependencies: libpthread,
	include_directories: srcdir,
	cpp_args: '-DFACTORY_TEST_APP'
)

srv_exe = executable(
	'process-server-test',
	sources: 'process_server_test.cc',
	dependencies: libpthread,
	include_directories: srcdir,
	cpp_args: [
		'-DFACTORY_TEST_SERVER',
		'-DFACTORY_APP_PATH=./src/test/process-server-test-app'
	]
)

#test('process-server', srv_exe)

executable(
	'posix-compliance',
	sources: 'posix_test.cc',
	include_directories: srcdir,
)
