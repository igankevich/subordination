project(
    'subordination',
    'cpp',
    version: '0.4.1',
    meson_version: '>=0.50.0',
    default_options: ['cpp_std=c++11'],
    license: 'gpl3+'
)

project_description = 'A framework for distributed programming'

with_debug = get_option('with_debug')
with_site = get_option('with_site')
with_examples = get_option('with_examples')
with_guile = get_option('with_guile')
with_python = get_option('with_python')
with_glusterfs = get_option('with_glusterfs')
with_dtests = get_option('with_dtests')

cpp = meson.get_compiler('cpp')

cpp_args = [
    '-Werror=return-type',
    '-Werror=return-local-addr',
    '-Werror=cast-align',
    '-Wcast-align=strict',
    '-Wno-terminate',
]
cpp_link_args = ['-rdynamic']
test_cpp_args = ['-DSBN_TEST']

# default configuration (for binaries that are installed)
binary_configs = [ {'cpp_args': [], 'prefix': ''}, ]
if with_dtests
    # test configuration (for binaries that are used in distributed tests)
    binary_configs += [ {'cpp_args': test_cpp_args, 'prefix': 'test_'}, ]
endif

if with_debug
    cpp_args += '-DSBN_DEBUG'
else
    cpp_args += '-fvisibility-inlines-hidden'
endif

if with_guile
    guile = dependency('guile-3.0')
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
unistdx_version = '>=0.66.0'
unistdx = with_debug ? dependency('unistdx-debug', version: unistdx_version) : dependency('unistdx', version: unistdx_version)
gtest = dependency('gtest', main: true)
guile = dependency('guile-3.0')

src = include_directories('src')
pkgconfig = import('pkgconfig')
clang_tidy_files = []

subdir('src')
subdir('guix')
subdir('ci')

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
