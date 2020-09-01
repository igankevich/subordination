config = configuration_data()
config.set('sharedstatedir', join_paths(get_option('sharedstatedir'), 'sbnd'))
config.set('SBND_WITH_GLUSTERFS', with_glusterfs)
configure_file(input: 'config.hh.in', output: 'config.hh', configuration: config)

sbnd_src = files([
    'hierarchy.cc',
    'hierarchy_kernel.cc',
    'hierarchy_node.cc',
    'job_status_kernel.cc',
    'pipeline_status_kernel.cc',
    'position_in_tree.cc',
    'process_pipeline.cc',
    'socket_pipeline.cc',
    'status_kernel.cc',
    'tree_hierarchy_iterator.cc',
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

sbnd_exe_src = files([
  'factory.cc',
  'master_discoverer.cc',
  'network_master.cc',
  'probe.cc',
  'prober.cc',
  'resident_kernel.cc',
  'sbnd.cc',
  'terminate_kernel.cc',
  'transaction_test_kernel.cc',
  'unix_socket_pipeline.cc',
])

if with_glusterfs
    subdir('glusterfs')
endif

sbnd_exe = executable(
    'sbnd',
    sources: sbnd_exe_src,
    include_directories: src,
    dependencies: [sbnd],
    install: true,
    install_dir: get_option('sbindir'),
    implicit_include_directories: false,
)

sbnc_exe = executable(
    'sbnc',
    sources: ['small_factory.cc', 'sbnc.cc', 'sbnc_transaction_test_kernel.cc'],
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

foreach name : ['local_server', 'tree_hierarchy_iterator', 'hierarchy']
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
        '--exit-code', '1',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=none',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=none',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

test(
    'daemon/socket-pipeline-slave-failure',
    dtest_exe,
    args: [
        '--exit-code', '1',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=slave',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=slave',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

test(
    'daemon/socket-pipeline-master-failure',
    dtest_exe,
    args: [
        '--exit-code', '2',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=master',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=master',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

test(
    'daemon/socket-pipeline-power-failure',
    dtest_exe,
    args: [
        '--exit-code', 'all',
        '--restart',
        '--size', '2',
        '--exec', '1', socket_pipeline_test.full_path(), 'role=master', 'failure=power',
        '--exec', '2', socket_pipeline_test.full_path(), 'role=slave', 'failure=power',
    ],
    workdir: meson.current_build_dir(),
    is_parallel: false
)

app_exe = executable(
    'process-pipeline-test-app',
    sources: 'process_pipeline_test.cc',
    dependencies: [sbn],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_APPLICATION'],
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
    'process-pipeline-test-daemon',
    sources: 'process_pipeline_test.cc',
    dependencies: [sbnd],
    include_directories: src,
    cpp_args: ['-DSUBORDINATION_TEST_DAEMON'],
    implicit_include_directories: false,
)

test('daemon/process-pipeline', daemon_exe)

test_config = configuration_data()
test_config.set('sbnd_exe_path', sbnd_exe.full_path())
test_config.set('sbnc_exe_path', sbnc_exe.full_path())
test_config.set('app_exe_path', test_app_exe.full_path())
configure_file(
    input: 'discovery_test.hh.in',
    output: 'discovery_test.hh',
    configuration: test_config
)

discovery_test_exe = executable(
    'discovery-test',
    sources: 'discovery_test.cc',
    include_directories: [src],
    dependencies: [sbn,dtest],
    implicit_include_directories: false,
)

foreach failure : ['no-failure', 'subordinate-failure', 'superior-failure']
    test(
        'daemon/discovery-' + failure,
        discovery_test_exe,
        args: [failure],
        workdir: meson.build_root(),
        is_parallel: false
    )
endforeach

test(
    'daemon/transaction',
    executable(
        'transaction-test',
        sources: 'transaction_test.cc',
        include_directories: [src],
        dependencies: [sbn,dtest],
        implicit_include_directories: false,
    ),
    workdir: meson.build_root(),
    is_parallel: false
)
