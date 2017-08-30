test(
	'factory::kstream',
	executable(
		'kstream-test',
		sources: 'kstream_test.cc',
		include_directories: srcdir,
		dependencies: [unistdx, gtest, factoryd]
	)
)

test_runner = executable(
	'test-runner',
	sources: 'test_runner.cc',
	include_directories: srcdir,
	dependencies: unistdx
)

executable(
	'nic-pipeline-test',
	sources: 'nic_pipeline_test.cc',
	dependencies: [threads, unistdx, gtest, factoryd],
	include_directories: srcdir,
)

executable(
	'nic-pipeline-offline-test',
	sources: 'nic_pipeline_test.cc',
	dependencies: [threads, unistdx, gtest, factoryd],
	include_directories: srcdir,
	cpp_args: [ '-DFACTORY_TEST_OFFLINE' ],
)

test(
	'factory::socket_pipeline (normal)',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', './nic-pipeline-test', 'role=master',
		'--exec', './nic-pipeline-test', 'role=slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::socket_pipeline (slave failure)',
	test_runner,
	args: [
		'--strategy=master-slave',
		'--exec', './nic-pipeline-offline-test', 'role=master',
		'--exec', './nic-pipeline-offline-test', 'role=slave'
	],
	workdir: meson.current_build_dir()
)

test(
	'factory::timer_pipeline',
	executable(
		'timer-pipeline-test',
		sources: 'timer_pipeline_test.cc',
		dependencies: [threads, unistdx, gtest, factoryd],
		include_directories: srcdir,
	)
)

app_exe = executable(
	'process-pipeline-test-app',
	sources: 'process_pipeline_test.cc',
	dependencies: [threads, unistdx, factory],
	include_directories: srcdir,
	cpp_args: [
		'-DFACTORY_APPLICATION',
		'-DFACTORY_TEST_APP',
	]
)

daemon_exe = executable(
	'process-pipeline-test',
	sources: 'process_pipeline_test.cc',
	dependencies: [threads, unistdx, factoryd],
	include_directories: srcdir,
	cpp_args: [
		'-DFACTORY_DAEMON',
		'-DFACTORY_TEST_SERVER',
		'-DFACTORY_APP_PATH=./src/test/process-pipeline-test-app',
	]
)

test('process-pipeline', daemon_exe)
