factoryd_src = files([
	'network_master.cc',
	'resident_kernel.cc',
	'hierarchy.cc',
	'factory.cc',
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
