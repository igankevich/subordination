configure_file(
	input: 'config.hh.in',
	output: 'config.hh',
	configuration: config
)

libfactory_src = files()
factory_core_src = files()

subdir('base')
subdir('kernel')
subdir('ppl')

factory_core_shared = shared_library(
	'factory-core',
	sources: factory_core_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	include_directories: srcdir,
)

factory_core = declare_dependency(
	link_with: factory_core_shared,
	include_directories: srcdir
)

libfactoryd_shared = shared_library(
	'factoryd',
	sources: libfactory_src,
	dependencies: [threads,unistdx,factory_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_DAEMON']
)

libfactorys_shared = shared_library(
	'factorys',
	sources: libfactory_src,
	dependencies: [threads,unistdx,factory_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_SUBMIT']
)

libfactory_shared = shared_library(
	'factory',
	sources: libfactory_src,
	dependencies: [threads,unistdx,factory_core],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir,
	cpp_args: ['-DFACTORY_APPLICATION']
)

factoryd = declare_dependency(
	link_with: [libfactoryd_shared,factory_core_shared],
	include_directories: srcdir
)

factorys = declare_dependency(
	link_with: [libfactorys_shared,factory_core_shared],
	include_directories: srcdir
)

factory = declare_dependency(
	link_with: [libfactory_shared,factory_core_shared],
	include_directories: srcdir
)
