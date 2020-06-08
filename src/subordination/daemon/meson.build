config = configuration_data()
config.set('sharedstatedir', get_option('sharedstatedir'))
configure_file(input: 'config.hh.in', output: 'config.hh', configuration: config)

sbnd_src = files([
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

sbnd_lib = library(
    'sbnd',
    sources: sbnd_src,
    dependencies: [sbn],
    version: meson.project_version(),
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

sbnd = declare_dependency(
    link_with: sbnd_lib,
    include_directories: src,
    dependencies: [sbn],
)

sbnd_exe = executable(
    'sbnd',
    sources: ['sbnd.cc'],
    include_directories: src,
    dependencies: [sbnd],
    cpp_args: profiling_args,
    install: true,
    install_dir: get_option('sbindir'),
    implicit_include_directories: false,
)

sbnc_exe = executable(
    'sbnc',
    sources: ['small_factory.cc', 'sbnc.cc'],
    include_directories: [src],
    dependencies: [sbnd],
    install: true,
    implicit_include_directories: false,
)

test_app_exe = executable(
    'test-application',
    sources: 'test_application.cc',
    include_directories: [src],
    dependencies: [sbn],
    implicit_include_directories: false,
)

foreach name : ['local_server', 'tree_hierarchy_iterator']
    test_name = '-'.join(name.split('_'))
    exe_name = test_name + '-test'
    exe = executable(
        exe_name,
        sources: name + '_test.cc',
        include_directories: src,
        dependencies: [sbnd, gtest],
        implicit_include_directories: false,
    )
    test('daemon/' + test_name, exe)
endforeach

socket_pipeline_test = executable(
    'socket-pipeline-test',
    sources: 'socket_pipeline_test.cc',
    dependencies: [gtest, sbnd],
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
    dependencies: [sbn],
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
    dependencies: [sbnd],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_SERVER'],
    implicit_include_directories: false,
)

test('daemon/process-pipeline', daemon_exe)

discovery_test_exe = executable(
    'discovery-test',
    sources: 'discovery_test.cc',
    include_directories: [src],
    dependencies: [sbn,gtest],
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
                        sbnd_exe.full_path(), 'fanout=' + fanout, 'allow_root=1',
                        # submit test application from the first node
                        '--exec', '1',
                        sbnc_exe.full_path(), test_app_exe.full_path(), failure
                    ],
                    env: env,
                    workdir: meson.build_root(),
                    is_parallel: false
                )
            endforeach
        endforeach
    endforeach
endif
