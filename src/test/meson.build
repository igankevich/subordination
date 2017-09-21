test(
	'kstream-test',
	executable(
		'kstream-test',
		sources: 'kstream_test.cc',
		include_directories: srcdir,
		dependencies: [unistdx, gtest, bscheduler_daemon],
		cpp_args: ['-DBSCHEDULER_DAEMON']
	)
)

test(
	'local-server-test',
	executable(
		'local-server-test',
		sources: 'local_server_test.cc',
		include_directories: srcdir,
		dependencies: [unistdx, gtest, bscheduler_daemon],
		cpp_args: ['-DBSCHEDULER_DAEMON']
	)
)

test_runner = executable(
	'test-runner',
	sources: 'test_runner.cc',
	include_directories: srcdir,
	dependencies: unistdx
)

socket_pipeline_test = executable(
	'socket-pipeline-test',
	sources: 'socket_pipeline_test.cc',
	dependencies: [threads, unistdx, gtest, bscheduler_daemon],
	include_directories: srcdir,
	cpp_args: ['-DBSCHEDULER_DAEMON']
)

test(
	'socket-pipeline-no-failure',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', socket_pipeline_test.full_path(), 'role=master', 'failure=no',
		'--exec', socket_pipeline_test.full_path(), 'role=slave', 'failure=no',
	],
	workdir: meson.current_build_dir()
)

test(
	'socket-pipeline-slave-failure',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', socket_pipeline_test.full_path(), 'role=master', 'failure=slave',
		'--exec', socket_pipeline_test.full_path(), 'role=slave', 'failure=slave',
	],
	workdir: meson.current_build_dir()
)

test(
	'timer-pipeline-test',
	executable(
		'timer-pipeline-test',
		sources: 'timer_pipeline_test.cc',
		dependencies: [threads, unistdx, gtest, bscheduler_daemon],
		include_directories: srcdir,
		cpp_args: ['-DBSCHEDULER_DAEMON']
	)
)

app_exe = executable(
	'process-pipeline-test-app',
	sources: 'process_pipeline_test.cc',
	dependencies: [threads, unistdx, bscheduler_app],
	include_directories: srcdir,
	cpp_args: [
		'-DBSCHEDULER_APPLICATION',
		'-DBSCHEDULER_TEST_APP',
	]
)

daemon_exe = executable(
	'process-pipeline-test',
	sources: 'process_pipeline_test.cc',
	dependencies: [threads, unistdx, bscheduler_daemon],
	include_directories: srcdir,
	cpp_args: [
		'-DBSCHEDULER_DAEMON',
		'-DBSCHEDULER_TEST_SERVER',
		'-DBSCHEDULER_APP_PATH=./src/test/process-pipeline-test-app',
	]
)

test('process-pipeline', daemon_exe)
