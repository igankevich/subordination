test(
	'discovery',
	executable(
		'discovery-test',
		sources: 'discovery_test.cc',
		include_directories: [srcdir, testdir],
		dependencies: threads,
		link_args: '-rdynamic'
	),
	args: [
		'--role', 'master',
		'--num-peers', '2',
		'--network', '127.0.0.1/255.0.0.0'
	]
)

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
