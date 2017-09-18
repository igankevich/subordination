factoryd_src = files([
	'factory.cc',
	'hierarchy.cc',
	'hierarchy_kernel.cc',
	'network_master.cc',
	'position_in_tree.cc',
	'tree_hierarchy_iterator.cc',
	'master_discoverer.cc',
	'prober.cc',
	'probe.cc',
	'resident_kernel.cc',
])

factoryd_exe = executable(
	'factory',
	sources: factoryd_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,factoryd],
	cpp_args: ['-DFACTORY_DAEMON'],
)

asub_src = files([
	'asub.cc',
])

executable(
	'asub',
	sources: asub_src,
	include_directories: [srcdir],
	dependencies: [unistdx,threads,factorys],
	cpp_args: ['-DFACTORY_SUBMIT'],
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
		dependencies: [gtest,unistdx,threads,factoryd],
		cpp_args: ['-DFACTORY_DEBUG_TREE_HIERARCHY_ITERATOR']
	)
)

test_env = environment()
test_env.set(
	'_CLUSTER',
	join_paths(meson.source_root(), 'src', 'test', 'cluster')
)
test_env.set('_LOGDIR', join_paths(meson.build_root(), 'logs'))
test_env.set('_FACTORY', factoryd_exe.full_path())

# run discovery test over virtual network
# using Linux namespaces
test(
	'discovery-test',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir],
		dependencies: [unistdx,threads,factoryd,gtest],
		cpp_args: ['-DFACTORY_DAEMON'],
	),
	args: [factoryd_exe.full_path()],
	env: test_env,
	workdir: meson.build_root()
)
