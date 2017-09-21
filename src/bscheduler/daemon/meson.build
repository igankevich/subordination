factoryd_src = files([
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
	'tree_hierarchy_iterator.cc',
])

factoryd_exe = executable(
	'factory',
	sources: factoryd_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_daemon],
	cpp_args: ['-DBSCHEDULER_DAEMON'],
)

asub_src = files([
	'asub.cc',
])

executable(
	'asub',
	sources: asub_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_submit],
	cpp_args: ['-DBSCHEDULER_SUBMIT'],
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
		dependencies: [gtest,unistdx,threads,bscheduler_daemon],
		cpp_args: ['-DBSCHEDULER_DEBUG_TREE_HIERARCHY_ITERATOR']
	)
)

discovery_test = executable(
	'discovery-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir],
	dependencies: [unistdx,threads,bscheduler_daemon,gtest],
	cpp_args: ['-DBSCHEDULER_DAEMON'],
)

foreach nodes : ['2', '4', '8']
	foreach fanout : ['1000', '2']
		suffix = '-' + nodes + '-' + fanout
		test_env = environment()
		test_env.set(
			'_CLUSTER',
			join_paths(meson.source_root(), 'src', 'test', 'cluster')
		)
		test_env.set('_LOGDIR', join_paths(meson.build_root(), 'logs' + suffix))
		test_env.set('_NODES', nodes)
		test_env.set('_FANOUT', fanout)
		test_env.set('_NAME', 'c' + suffix + '-')
		# run discovery test over virtual network
		# using Linux namespaces
		test(
			'discovery-test' + suffix,
			discovery_test,
			args: [factoryd_exe.full_path(), 'fanout=' + fanout],
			env: test_env,
			workdir: meson.build_root()
		)
	endforeach
endforeach
