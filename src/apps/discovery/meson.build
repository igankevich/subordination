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
#		dependencies: [libpthread, librt, libgsl, liblapack, libblas],
		link_args: ['-static-libgcc', '-static-libstdc++', '-static', '-llapack', '-lblas', '-lgfortran', '-lquadmath', '-lpthread' , '-lm'],
		cpp_args: ['-DFACTORY_SINGLE_THREAD', '-DFACTORY_PRIORITY_SCHEDULING']
	)
#	,
#	args: [
#		'--role', 'master',
#		'--num-peers', '2',
#		'--network', '127.0.0.1/8'
#	]
#)

test('discovery-ranking',
	executable(
		'discovery-ranking-test',
		sources: 'discovery_ranking_test.cc',
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
