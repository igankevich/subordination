configure_file(
	input: 'config.hh.in',
	output: 'config.hh',
	configuration: config
)

libfactory_src = files()

subdir('base')
subdir('kernel')
subdir('ppl')

libfactoryd_shared = shared_library(
	'factoryd',
	sources: libfactory_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_DAEMON']
)

libfactorys_shared = shared_library(
	'factorys',
	sources: libfactory_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_SUBMIT']
)

libfactory_shared = shared_library(
	'factory',
	sources: libfactory_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_APPLICATION']
)

factoryd = declare_dependency(
	link_with: libfactoryd_shared,
	include_directories: srcdir
)

factorys = declare_dependency(
	link_with: libfactorys_shared,
	include_directories: srcdir
)

factory = declare_dependency(
	link_with: libfactory_shared,
	include_directories: srcdir
)
