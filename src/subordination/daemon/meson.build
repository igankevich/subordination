subordination_src = files([
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
])

subordination_exe = executable(
    'subordination',
    sources: subordination_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_daemon],
    cpp_args: ['-DSUBORDINATION_DAEMON'] + profiling_args,
    install: true,
    install_dir: get_option('sbindir')
)

submit_src = files([
    'submit.cc',
])

submit_exe = executable(
    'submit',
    sources: submit_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_submit],
    cpp_args: ['-DSUBORDINATION_SUBMIT'],
    install: true
)

status_src = files([
    'status.cc',
    'status_kernel.cc',
    'hierarchy.cc',
    'hierarchy_node.cc',
])

status_exe = executable(
    'status',
    sources: status_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_submit],
    cpp_args: ['-DSUBORDINATION_SUBMIT'],
    install: true
)

test_app_exe = executable(
    'test-application',
    sources: 'test_application.cc',
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_app],
    cpp_args: ['-DSUBORDINATION_APPLICATION'],
)

test(
    'tree-hierarchy-iterator-test',
    executable(
        'tree-hierarchy-iterator-test',
        sources: [
            'position_in_tree.cc',
            'tree_hierarchy_iterator.cc',
            'tree_hierarchy_iterator_test.cc'
        ],
        dependencies: [gtest,unistdx,threads,subordination_daemon],
        cpp_args: ['-DSUBORDINATION_DEBUG_TREE_HIERARCHY_ITERATOR']
    )
)


discovery_test_exe = executable(
    'discovery-test',
    sources: 'discovery_test.cc',
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_daemon,gtest],
    cpp_args: ['-DSUBORDINATION_DAEMON'],
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
