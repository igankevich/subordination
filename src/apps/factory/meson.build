factoryd_src = files([
	'network_master.cc',
	'factory.cc',
])

executable(
	'factory',
	sources: factoryd_src,
	include_directories: [srcdir, testdir],
	dependencies: [unistdx,threads,factory]
)
