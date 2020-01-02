project(
    'subordination',
    'cpp',
    version: '0.2.4',
    meson_version: '>=0.46.0',
    default_options: ['cpp_std=c++11']
)

cpp = meson.get_compiler('cpp')

if not get_option('buildtype').contains('debug')
    add_global_arguments('-DNDEBUG', language: 'cpp')
endif
#add_global_arguments('-DUNISTDX_DEBUG_PACKETBUF', language: 'cpp')
#add_global_arguments('-DUNISTDX_DEBUG_FILDESBUF', language: 'cpp')

foreach arg : [
    '-Werror=return-type',
    '-Werror=return-local-addr',
    '-Wcast-align']
    if cpp.has_argument(arg)
        add_global_arguments(arg, language: 'cpp')
    endif
endforeach

with_doc = get_option('with_doc')
with_rpm = get_option('with_rpm')

profiling_args = []
if get_option('profile_node_discovery')
    profiling_args += ['-DSUBORDINATION_PROFILE_NODE_DISCOVERY']
endif

if cpp.get_id() == 'gcc' or cpp.has_argument('-rdynamic')
    add_global_link_arguments('-rdynamic', language: 'cpp')
endif

# configuration
config = configuration_data()
config.set('kernel_type', get_option('kernel_type'))
config.set('kernel_header', get_option('kernel_header'))

threads = dependency('threads')
unistdx = dependency('unistdx', version: '>=0.4.15')
gtest = dependency('gtest', main: true)

srcdir = include_directories('src')

subdir('src')

if with_rpm
    subdir('rpm')
endif

if with_doc
    subdir('doc')
endif

# cppcheck {{{
cppcheck = find_program('cppcheck', required: false)
if cppcheck.found()
    run_target(
        'cppcheck',
        command: [
            cppcheck,
            '--enable=all',
            '--quiet',
            '--force',
            '--language=c++',
            '--std=c++11',
            '--template=gcc',
            '-I' + join_paths(meson.source_root(), 'src'),
            '-I' + join_paths(meson.build_root(), 'src'),
            join_paths(meson.source_root(), 'src'),
        ]
    )
endif
# }}}
# pkgconfig {{{
pkgconfig = import('pkgconfig')
pkgconfig.generate(
    subordination_core_lib,
    requires: [unistdx],
    version: meson.project_version(),
    name: 'subordination-core',
    filebase: 'subordination-core',
    description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
    subordination_app_lib,
    libraries: [subordination_core_lib],
    requires: [unistdx],
    extra_cflags: '-DSUBORDINATION_APPLICATION',
    version: meson.project_version(),
    name: 'subordination-app',
    filebase: 'subordination-app',
    description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
    subordination_daemon_lib,
    libraries: [subordination_core_lib],
    requires: [unistdx],
    extra_cflags: '-DSUBORDINATION_DAEMON',
    version: meson.project_version(),
    name: 'subordination-daemon',
    filebase: 'subordination-daemon',
    description: 'Distributed programming C++ framework',
)
pkgconfig.generate(
    subordination_submit_lib,
    libraries: [subordination_core_lib],
    requires: [unistdx],
    extra_cflags: '-DSUBORDINATION_SUBMIT',
    version: meson.project_version(),
    name: 'subordination-submit',
    filebase: 'subordination-submit',
    description: 'Distributed programming C++ framework',
)
# }}}
