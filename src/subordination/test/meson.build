app_exe = executable(
    'process-pipeline-test-app',
    sources: 'process_pipeline_test.cc',
    dependencies: [threads, unistdx, subordination_core],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_APP'],
    implicit_include_directories: false,
)

empty_exe = executable('empty', sources: 'empty.cc')
test_config = configuration_data()
test_config.set('empty_exe_path', empty_exe.full_path())
test_config.set('app_exe_path', app_exe.full_path())
configure_file(
    input: 'config.hh.in',
    output: 'config.hh',
    configuration: test_config
)

daemon_exe = executable(
    'process-pipeline-test',
    sources: 'process_pipeline_test.cc',
    dependencies: [threads, unistdx, subordination_core],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_SERVER'],
    implicit_include_directories: false,
)

test('process-pipeline', daemon_exe)
