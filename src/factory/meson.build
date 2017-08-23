libfactory_src = files()

subdir('bits')

libfactory_src += files([
	'error.cc',
	'kernel_error.cc',
	'type.cc',
])

libfactory_shared = shared_library(
	'factory',
	sources: libfactory_src,
	dependencies: unistdx,
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

