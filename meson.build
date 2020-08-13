project(
    'subordination',
    'cpp',
    version: '0.3.1',
    meson_version: '>=0.47.0',
    default_options: ['cpp_std=c++11'],
    license: 'gpl3+'
)

project_description = 'A framework for distributed programming'

with_debug = get_option('buildtype').contains('debug')
with_site = get_option('with_site')
with_rpm = get_option('with_rpm')
with_examples = get_option('with_examples')

cpp = meson.get_compiler('cpp')

cpp_args = ['-Werror=return-type', '-Werror=return-local-addr', '-Werror=cast-align']
cpp_link_args = ['-rdynamic']

if with_debug
    cpp_args += '-DSBN_DEBUG'
    cpp_args += '-DUNW_LOCAL_ONLY'
else
    cpp_args += '-fvisibility-inlines-hidden'
endif

foreach arg : cpp_args
    if cpp.has_argument(arg)
        add_global_arguments(arg, language: 'cpp')
    endif
endforeach

foreach arg : cpp_link_args
    if cpp.has_link_argument(arg)
        add_global_link_arguments(arg, language: 'cpp')
    endif
endforeach

threads = dependency('threads')
unistdx = dependency('unistdx', version: '>=0.22.0')
gtest = dependency('gtest', main: true)
libunwind = with_debug ? [dependency('libunwind')] : []
libdw = with_debug ? [dependency('libdw')] : []

src = include_directories('src')
pkgconfig = import('pkgconfig')

subdir('src')
subdir('guix')

if with_rpm
    subdir('rpm')
endif

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
