test(
    'kstream-test',
    executable(
        'kstream-test',
        sources: 'kstream_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_core],
        implicit_include_directories: false,
    )
)

test(
    'local-server-test',
    executable(
        'local-server-test',
        sources: 'local_server_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_core],
        implicit_include_directories: false,
    )
)

socket_pipeline_test = executable(
    'socket-pipeline-test',
    sources: 'socket_pipeline_test.cc',
    dependencies: [threads, unistdx, gtest, subordination_core],
    include_directories: src,
    implicit_include_directories: false,
)

test(
    'socket-pipeline-no-failure',
    dtest_exe,
    args: [
        '--exit-code', 'master',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=no',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=no',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

test(
    'socket-pipeline-slave-failure',
    dtest_exe,
    args: [
        '--exit-code', 'master',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=slave',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=slave', 'failure=slave',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

test(
    'timer-pipeline-test',
    executable(
        'timer-pipeline-test',
        sources: 'timer_pipeline_test.cc',
        dependencies: [threads, unistdx, gtest, subordination_core],
        include_directories: src,
    )
)

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
