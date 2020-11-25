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

if with_dtests
    test_config = configuration_data()
    test_config.set('sbnd_exe_path', test_sbnd_exe.full_path())
    test_config.set('sbnc_exe_path', test_sbnc_exe.full_path())
    test_config.set('sbn_python_exe_path', sbn_python_exe.full_path())
    test_config.set('pyfile_path', join_paths(meson.current_source_dir(), 'test.py'))
    configure_file(
        input: 'python_remote_test.hh.in',
        output: 'python_remote_test.hh',
        configuration: test_config
    )

    test(
        'python/remote',
        executable(
            'python-remote-test',
            sources: 'python_remote_test.cc',
            cpp_args: test_cpp_args,
            include_directories: [src],
            dependencies: [test_sbn,dtest] + valgrind_dep,
            implicit_include_directories: false,
        ),
        workdir: meson.build_root(),
        is_parallel: false
    )
endif

test(
    'python/local',
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
