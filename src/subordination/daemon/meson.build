subordination_src = files([
    'subordination.cc',
    'hierarchy.cc',
    'hierarchy_kernel.cc',
    'hierarchy_node.cc',
    'master_discoverer.cc',
    'network_master.cc',
    'position_in_tree.cc',
    'probe.cc',
    'prober.cc',
    'resident_kernel.cc',
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

bsub_src = files([
    'submit.cc',
])

bsub_exe = executable(
    'submit',
    sources: bsub_src,
    include_directories: [src],
    dependencies: [unistdx,threads,subordination_submit],
    cpp_args: ['-DSUBORDINATION_SUBMIT'],
    install: true
)

test_application = executable(
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

# run discovery test over virtual network
# using Linux namespaces
# TODO fix the test
if get_option('buildtype').contains('debug') and false
    discovery_test = executable(
        'discovery-test',
        sources: 'discovery_test.cc',
        include_directories: [src],
        dependencies: [unistdx,threads,subordination_daemon,gtest],
        cpp_args: ['-DSUBORDINATION_DAEMON'],
    )
    foreach nodes : ['2', '4', '8']
        foreach fanout : ['1000', '2']
            suffix = '-' + nodes + '-' + fanout
            if nodes == '2' and fanout == '2'
                foreach arg : ['no-failure', 'slave-failure', 'master-failure']
                    test_env = environment()
                    test_env.set(
                        '_CLUSTER',
                        join_paths(meson.source_root(), 'src', 'test', 'cluster')
                    )
                    test_env.set('_LOGDIR', join_paths(meson.build_root(), 'logs' + suffix))
                    test_env.set('_NODES', nodes)
                    test_env.set('_FANOUT', fanout)
                    test_env.set('_NAME', 'c' + suffix + '-')
                    test_env.set('_FAILURE', arg)
                    test_env.set(
                        '_SUBMIT',
                        bsub_exe.full_path() + ' ' + test_application.full_path() + ' ' + arg
                    )
                    test(
                        'discovery-test' + suffix + '-' + arg,
                        discovery_test,
                        args: [subordination_exe.full_path(), 'fanout=' + fanout, 'allow_root=1'],
                        env: test_env,
                        workdir: meson.build_root()
                    )
                endforeach
            else
                test_env = environment()
                test_env.set(
                    '_CLUSTER',
                    join_paths(meson.source_root(), 'src', 'test', 'cluster')
                )
                test_env.set('_LOGDIR', join_paths(meson.build_root(), 'logs' + suffix))
                test_env.set('_NODES', nodes)
                test_env.set('_FANOUT', fanout)
                test_env.set('_NAME', 'c' + suffix + '-')
                test(
                    'discovery-test' + suffix,
                    discovery_test,
                    args: [subordination_exe.full_path(), 'fanout=' + fanout, 'allow_root=1'],
                    env: test_env,
                    workdir: meson.build_root()
                )
            endif
        endforeach
    endforeach
endif
