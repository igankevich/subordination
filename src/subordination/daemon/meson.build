subordination_daemon_core_src = files([
    'application_kernel.cc',
    'factory.cc',
    'hierarchy.cc',
    'hierarchy_kernel.cc',
    'hierarchy_node.cc',
    'master_discoverer.cc',
    'network_master.cc',
    'position_in_tree.cc',
    'probe.cc',
    'prober.cc',
    'process_pipeline.cc',
    'resident_kernel.cc',
    'socket_pipeline.cc',
    'status_kernel.cc',
    'tree_hierarchy_iterator.cc',
    'unix_socket_pipeline.cc',
])

subordination_daemon_core_lib = library(
    'subordination-daemon-core',
    sources: subordination_daemon_core_src,
    dependencies: [subordination_core],
    version: meson.project_version(),
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

subordination_daemon_core = declare_dependency(
    link_with: subordination_daemon_core_lib,
    include_directories: src,
    dependencies: [subordination_core],
)

subordination_exe = executable(
    'subordination',
    sources: ['subordination.cc'],
    include_directories: src,
    dependencies: [subordination_daemon_core],
    cpp_args: profiling_args,
    install: true,
    install_dir: get_option('sbindir'),
    implicit_include_directories: false,
)

submit_src = files([
    'small_factory.cc',
    'submit.cc',
])

submit_exe = executable(
    'submit',
    sources: submit_src,
    include_directories: [src],
    dependencies: [subordination_daemon_core],
    install: true,
    implicit_include_directories: false,
)

status_src = files([
    'status.cc',
    'small_factory.cc',
])

status_exe = executable(
    'status',
    sources: status_src,
    include_directories: [src],
    dependencies: [subordination_daemon_core],
    install: true,
    implicit_include_directories: false,
)

test_app_exe = executable(
    'test-application',
    sources: 'test_application.cc',
    include_directories: [src],
    dependencies: [subordination_core],
    implicit_include_directories: false,
)

foreach name : ['local_server', 'tree_hierarchy_iterator']
    test_name = '-'.join(name.split('_'))
    exe_name = test_name + '-test'
    exe = executable(
        exe_name,
        sources: name + '_test.cc',
        include_directories: src,
        dependencies: [subordination_daemon_core, gtest],
        implicit_include_directories: false,
    )
    test('daemon/' + test_name, exe)
endforeach

socket_pipeline_test = executable(
    'socket-pipeline-test',
    sources: 'socket_pipeline_test.cc',
    dependencies: [gtest, subordination_daemon_core],
    include_directories: src,
    implicit_include_directories: false,
)

test(
    'daemon/socket-pipeline-no-failure',
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
    'daemon/socket-pipeline-slave-failure',
    dtest_exe,
    args: [
        '--exit-code', 'master',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=slave',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=slave',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

app_exe = executable(
    'process-pipeline-test-app',
    sources: 'process_pipeline_test.cc',
    dependencies: [subordination_core],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_APP'],
    implicit_include_directories: false,
)

test_config = configuration_data()
test_config.set('app_exe_path', app_exe.full_path())
configure_file(
    input: 'process_pipeline_test.hh.in',
    output: 'process_pipeline_test.hh',
    configuration: test_config
)

daemon_exe = executable(
    'process-pipeline-test',
    sources: 'process_pipeline_test.cc',
    dependencies: [subordination_daemon_core],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_SERVER'],
    implicit_include_directories: false,
)

test('daemon/process-pipeline', daemon_exe)

discovery_test_exe = executable(
    'discovery-test',
    sources: 'discovery_test.cc',
    include_directories: [src],
    dependencies: [subordination_core,gtest],
    implicit_include_directories: false,
)

if get_option('b_sanitize') != 'address'
    foreach nodes : ['2', '4']
        foreach fanout : ['2', '1000']
            foreach failure : ['no-failure', 'slave-failure', 'master-failure']
                suffix = '-' + nodes + '-' + fanout + '-' + failure
                env = environment()
                env.set('DTEST_EXE', dtest_exe.full_path())
                env.set('DTEST_NAME', 'x')
                env.set('DTEST_SIZE', nodes)
                env.set('SBN_FANOUT', fanout)
                env.set('SBN_FAILURE', failure)
                # test executable is a wrapper for dtest
                # (all arguments are forwarded to dtest)
                test(
                    'daemon/discovery' + suffix,
                    discovery_test_exe,
                    args: [
                        # run daemon on each cluster node
                        '--exec', '*',
                        subordination_exe.full_path(), 'fanout=' + fanout, 'allow_root=1',
                        # submit test application from the first node
                        '--exec', '1',
                        submit_exe.full_path(), test_app_exe.full_path(), failure
                    ],
                    env: env,
                    workdir: meson.build_root(),
                    is_parallel: false
                )
            endforeach
        endforeach
    endforeach
endif
