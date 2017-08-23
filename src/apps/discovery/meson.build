libz = dependency('zlib')

#test(
#	'discovery',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: [threads, libz, unistdx],
		cpp_args: ['-DFACTORY_SINGLE_THREAD', '-DFACTORY_PRIORITY_SCHEDULING'],
	)
#	,
#	args: [
#		'--role', 'master',
#		'--num-peers', '2',
#		'--network', '127.0.0.1/8'
#	]
#)
executable(
	'slave-failure-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir, testdir],
	dependencies: [threads, libz, unistdx],
	cpp_args: [
		'-DFACTORY_PRIORITY_SCHEDULING',
		'-DFACTORY_TEST_SLAVE_FAILURE',
		'-DFACTORY_TEST_USE_SSH'
	],
)

executable(
	'master-failure-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir, testdir],
	dependencies: [threads, libz, unistdx],
	cpp_args: [
		'-DFACTORY_PRIORITY_SCHEDULING',
		'-DFACTORY_TEST_USE_SSH'
	],
)

test(
	'discovery::Distance_in_tree',
	executable(
		'discovery-ranking-test',
		sources: 'discovery_ranking_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: [unistdx, gtest]
	)
)

test(
	'discovery::csv_tuple',
	executable(
		'csv-test',
		sources: 'csv_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: [unistdx, gtest]
	)
)

executable(
	'discover-topology',
	sources: 'discover_topology.cc',
	include_directories: [srcdir, testdir],
	dependencies: [unistdx]
)
