libgsl = dependency('gsl')
liblapack = find_library('lapack')
libblas = find_library('blas')

#test(
#	'discovery',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir, testdir],
#		dependencies: threads,
#		link_args: '-rdynamic'
		dependencies: [libpthread, librt, libgsl, liblapack, libblas],
		link_args: ['-rdynamic', '-fopenmp', '-lgfortran'],
		cpp_args: ['-fopenmp', '-DFACTORY_SINGLE_THREAD']
	)
#	,
#	args: [
#		'--role', 'master',
#		'--num-peers', '2',
#		'--network', '127.0.0.1/8'
#	]
#)

test('network',
	executable(
		'network-test',
		sources: 'network_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: threads,
		link_args: '-rdynamic'
	)
)

run_target(
	'discovery-animation',
	meson.source_root() + '/scripts/discovery-animation'
)

test('csv',
	executable(
		'csv-test',
		sources: 'csv_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: threads
	)
)

executable(
	'discover-topology',
	sources: 'discover_topology.cc',
	include_directories: [srcdir, testdir],
	dependencies: threads,
	link_args: '-rdynamic'
)
