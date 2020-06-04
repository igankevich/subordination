project(
    'subordination',
    'cpp',
    version: '0.2.5',
    meson_version: '>=0.47.0',
    default_options: ['cpp_std=c++11'],
    license: 'gpl3+'
)

project_description = 'A framework for distributed programming'

cpp = meson.get_compiler('cpp')

if get_option('buildtype').contains('debug')
    add_global_arguments('-DSBN_DEBUG', language: 'cpp')
else
    arg = '-fvisibility-inlines-hidden'
    if cpp.has_argument(arg)
        add_global_arguments(arg, language: 'cpp')
    endif
endif
#add_global_arguments('-DUNISTDX_DEBUG_PACKETBUF', language: 'cpp')
#add_global_arguments('-DUNISTDX_DEBUG_FILDESBUF', language: 'cpp')
#add_global_arguments('-DUNISTDX_DEBUG_EVENT_POLLER', language: 'cpp')

foreach arg : ['-Werror=return-type', '-Werror=return-local-addr', '-Werror=cast-align']
    if cpp.has_argument(arg)
        add_global_arguments(arg, language: 'cpp')
    endif
endforeach

if cpp.has_link_argument('-rdynamic')
    add_global_link_arguments('-rdynamic', language: 'cpp')
endif

with_site = get_option('with_site')
with_rpm = get_option('with_rpm')

profiling_args = []
if get_option('profile_node_discovery')
    profiling_args += ['-DSUBORDINATION_PROFILE_NODE_DISCOVERY']
endif

threads = dependency('threads')
unistdx = dependency('unistdx', version: '>=0.20.4')
gtest = dependency('gtest', main: true)

src = include_directories('src')
pkgconfig = import('pkgconfig')

subdir('src')

if with_rpm
    subdir('rpm')
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
