libz = dependency('zlib')

#test(
#	'discovery',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: [libpthread, libz, unistdx],
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
	dependencies: [libpthread, libz, unistdx],
	cpp_args: [
		'-DFACTORY_PRIORITY_SCHEDULING',
		'-DFACTORY_TEST_SLAVE_FAILURE'
	],
)

executable(
	'master-failure-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir, testdir],
	dependencies: [libpthread, libz, unistdx],
	cpp_args: ['-DFACTORY_PRIORITY_SCHEDULING'],
)

test(
	'discovery::Distance_in_tree',
	executable(
		'discovery-ranking-test',
		sources: 'discovery_ranking_test.cc',
		dependencies: [unistdx],
		include_directories: [srcdir, testdir],
	)
)

test(
	'discovery::csv_tuple',
	executable(
		'csv-test',
		sources: 'csv_test.cc',
		dependencies: [unistdx],
		include_directories: [srcdir, testdir],
	)
)

executable(
	'discover-topology',
	sources: 'discover_topology.cc',
	dependencies: [unistdx],
	include_directories: [srcdir, testdir],
)
