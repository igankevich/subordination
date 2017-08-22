test(
	'factory::Kernel_stream',
	executable(
		'kernel-stream-test',
		sources: 'kernel_stream_test.cc',
		include_directories: srcdir,
		dependencies: unistdx
	)
)

test_runner = executable(
	'test-runner',
	sources: 'test_runner.cc',
	include_directories: srcdir,
	dependencies: unistdx
)

executable(
	'nic-server-test',
	sources: 'nic_server_test.cc',
	dependencies: [threads, unistdx],
	include_directories: srcdir,
)

executable(
	'nic-server-offline-test',
	sources: 'nic_server_test.cc',
	dependencies: [threads, unistdx],
	include_directories: srcdir,
	cpp_args: [ '-DFACTORY_TEST_OFFLINE' ],
)

test(
	'factory::NIC_server (normal)',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', './nic-server-test', 'role=master',
		'--exec', './nic-server-test', 'role=slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::NIC_server (slave failure)',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', './nic-server-offline-test', 'role=master',
		'--exec', './nic-server-offline-test', 'role=slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::Timer_server',
	executable(
		'timer-server-test',
		sources: 'timer_server_test.cc',
		dependencies: [threads, unistdx],
		include_directories: srcdir,
	)
)

app_exe = executable(
	'process-server-test-app',
	sources: 'process_server_test.cc',
	dependencies: [threads, unistdx],
	include_directories: srcdir,
	cpp_args: '-DFACTORY_TEST_APP'
)

srv_exe = executable(
	'process-server-test',
	sources: 'process_server_test.cc',
	dependencies: [threads, unistdx],
	include_directories: srcdir,
	cpp_args: [
		'-DFACTORY_TEST_SERVER',
		'-DFACTORY_APP_PATH=./src/test/process-server-test-app'
	]
)

#test('process-server', srv_exe)
