subordination_src = files([
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
    'resident_kernel.cc',
    'status_kernel.cc',
    'subordination.cc',
    'tree_hierarchy_iterator.cc',
    'unix_socket_pipeline.cc',
])

subordination_exe = executable(
    'subordination',
    sources: subordination_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_core],
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
    dependencies: [unistdx,threads,subordination_core],
    install: true,
    implicit_include_directories: false,
)

status_src = files([
    'status.cc',
    'status_kernel.cc',
    'hierarchy.cc',
    'hierarchy_node.cc',
    'small_factory.cc',
])

status_exe = executable(
    'status',
    sources: status_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_core],
    install: true,
    implicit_include_directories: false,
)

test_app_exe = executable(
    'test-application',
    sources: 'test_application.cc',
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_core],
    implicit_include_directories: false,
)

test(
    'daemon/tree-hierarchy-iterator',
    executable(
        'tree-hierarchy-iterator-test',
        sources: [
            'position_in_tree.cc',
            'tree_hierarchy_iterator.cc',
            'tree_hierarchy_iterator_test.cc'
        ],
        dependencies: [gtest,unistdx,threads,subordination_core],
        cpp_args: ['-DSUBORDINATION_DEBUG_TREE_HIERARCHY_ITERATOR'],
        implicit_include_directories: false,
    )
)


discovery_test_exe = executable(
    'discovery-test',
    sources: 'discovery_test.cc',
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_core,gtest],
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
                    'discovery-test' + suffix,
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
