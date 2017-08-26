configure_file(
	input: 'config.hh.in',
	output: 'config.hh',
	configuration: config
)

libfactory_src = files()

subdir('base')
subdir('reg')
subdir('kernel')
subdir('ppl')

libfactory_src += files([
	'error.cc',
	'registry.cc',
])

if get_option('springy')
	libfactory_src += springy_src
endif

libfactory_shared = shared_library(
	'factory',
	sources: libfactory_src,
	dependencies: [threads,unistdx],
	version: meson.project_version(),
	install: true,
	include_directories: srcdir
)

libfactory_static = static_library(
	'factory',
	sources: libfactory_src,
	dependencies: unistdx,
	install: true,
	include_directories: srcdir
)

factory = declare_dependency(
	link_with: libfactory_static,
	include_directories: srcdir
)

