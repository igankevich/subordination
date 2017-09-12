factoryd_src = files([
	'factory.cc',
	'hierarchy.cc',
	'network_master.cc',
	'position_in_tree.cc',
	'tree_hierarchy_iterator.cc',
	'master_discoverer.cc',
	'prober.cc',
	'probe.cc',
	'resident_kernel.cc',
])

executable(
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
