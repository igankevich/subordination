test('discovery', executable(
	'discovery-test',
	sources: 'discovery_test.cc',
	include_directories: [srcdir, testdir],
	dependencies: threads,
	link_args: '-rdynamic')
)

test('network', executable(
	'network-test',
	sources: 'network_test.cc',
	include_directories: [srcdir, testdir],
	dependencies: threads,
	link_args: '-rdynamic')
)
