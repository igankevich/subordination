sbn_python_src = files([
    'kernel.cc',
    'init.cc',
    'python.cc',
])

clang_tidy_files += sbn_python_src

sbn_python_deps = [
    sbn,
    threads,
    unistdx,
    dependency('python3-embed'),
    dependency('zlib')
]

sbn_python_exe = executable(
    'sbn-python3',
    sources: sbn_python_src,
    dependencies: sbn_python_deps,
    include_directories: src,
    install: true,
    implicit_include_directories: false,
    cpp_args: ['-DPY_SSIZE_T_CLEAN'],
)

test(
    'python',
    executable(
        'python-local-test',
        sources: ['python_local_test.cc'],
        dependencies: [unistdx] + valgrind_dep,
        include_directories: src,
        implicit_include_directories: false,
    ),
    args: [sbn_python_exe, join_paths(meson.current_source_dir(), 'test.py')],
    env: test_env,
)
