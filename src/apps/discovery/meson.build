liblapack = cpp.find_library('lapack')
libblas = cpp.find_library('blas')
libz = dependency('zlib')
static_linking = ['-static-libgcc', '-static-libstdc++', '-static', '-llapack', '-lblas', '-lgfortran', '-lquadmath', '-lpthread' , '-lm', '-lz']
if host_machine.system() == 'darwin'
	static_linking = ['-llapack', '-lblas', '-lpthread' , '-lm', '-lz']
endif

#test(
#	'discovery',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir, testdir],
#		dependencies: threads,
#		dependencies: [libpthread, librt, libgsl, liblapack, libblas],
		link_args: static_linking,
		cpp_args: ['-DFACTORY_SINGLE_THREAD', '-DFACTORY_PRIORITY_SCHEDULING']
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
	link_args: static_linking,
	cpp_args: [
		'-DFACTORY_PRIORITY_SCHEDULING',
		'-DFACTORY_TEST_SLAVE_FAILURE',
		'-DFACTORY_TEST_USE_SSH'
	]
)

executable(
	'master-failure-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir, testdir],
	link_args: static_linking,
	cpp_args: [
		'-DFACTORY_PRIORITY_SCHEDULING',
		'-DFACTORY_TEST_USE_SSH'
	]
)

test('discovery-ranking',
	executable(
		'discovery-ranking-test',
		sources: 'discovery_ranking_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: threads
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
	dependencies: threads
)
