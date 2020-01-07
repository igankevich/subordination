test(
    'queue-pusher-test',
    executable(
        'queue-pusher-test',
        sources: 'queue_pusher_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_core]
    )
)

test(
    'kstream-test',
    executable(
        'kstream-test',
        sources: 'kstream_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_daemon],
        cpp_args: ['-DSUBORDINATION_DAEMON']
    )
)

test(
    'local-server-test',
    executable(
        'local-server-test',
        sources: 'local_server_test.cc',
        include_directories: src,
        dependencies: [unistdx, gtest, subordination_daemon],
        cpp_args: ['-DSUBORDINATION_DAEMON']
    )
)

socket_pipeline_test = executable(
    'socket-pipeline-test',
    sources: 'socket_pipeline_test.cc',
    dependencies: [threads, unistdx, gtest, subordination_daemon],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_DAEMON']
)

test(
    'socket-pipeline-no-failure',
    dtest_exe,
    args: [
        '--exit-code=master',
        '--size=2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=no',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=no',
    ],
    workdir: meson.current_build_dir()
)

test(
    'socket-pipeline-slave-failure',
    dtest_exe,
    args: [
        '--exit-code=master',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=slave',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=slave', 'failure=slave',
    ],
    workdir: meson.current_build_dir()
)

test(
    'timer-pipeline-test',
    executable(
        'timer-pipeline-test',
        sources: 'timer_pipeline_test.cc',
        dependencies: [threads, unistdx, gtest, subordination_daemon],
        include_directories: src,
        cpp_args: ['-DSUBORDINATION_DAEMON']
    )
)

app_exe = executable(
    'process-pipeline-test-app',
    sources: 'process_pipeline_test.cc',
    dependencies: [threads, unistdx, subordination_app],
    include_directories: src,
    cpp_args: [
        '-DSUBORDINATION_APPLICATION',
        '-DSUBORDINATION_TEST_APP',
    ]
)

daemon_exe = executable(
    'process-pipeline-test',
    sources: 'process_pipeline_test.cc',
    dependencies: [threads, unistdx, subordination_daemon],
    include_directories: src,
    cpp_args: [
        '-DSUBORDINATION_DAEMON',
        '-DSUBORDINATION_TEST_SERVER',
        '-DSUBORDINATION_APP_PATH=' + app_exe.full_path()
    ]
)

test('process-pipeline', daemon_exe)
