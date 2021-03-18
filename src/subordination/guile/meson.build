sbn_guile_src = files([
    'base.cc',
    'init.cc',
    'kernel.cc',
    'main.cc',
])

clang_tidy_files += sbn_guile_src

sbn_guile_deps = [sbn,threads,unistdx,guile,dependency('zlib')]

sbn_guile_exe = executable(
    'sbn-guile',
    sources: sbn_guile_src,
    dependencies: sbn_guile_deps,
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)

test(
    'guile/local',
    executable(
        'local-test',
        sources: ['local_test.cc'],
        dependencies: [unistdx] + valgrind_dep,
        include_directories: src,
        implicit_include_directories: false,
    ),
    args: [sbn_guile_exe, join_paths(meson.current_source_dir(), 'test.scm')],
    env: test_env,
)

test(
    'guile/object-read-write',
    executable(
        'object-read-write-test',
        sources: ['object_read_write_test.cc','kernel.cc','base.cc','init.cc'],
        dependencies: [unistdx,gtest] + sbn_guile_deps + valgrind_dep,
        include_directories: src,
        implicit_include_directories: false,
    ),
    env: test_env,
)

if with_dtests
    test_config = configuration_data()
    test_config.set('sbnd_exe_path', test_sbnd_exe.full_path())
    test_config.set('sbnc_exe_path', test_sbnc_exe.full_path())
    test_config.set('sbn_guile_exe_path', sbn_guile_exe.full_path())
    test_config.set('test_scm_path', join_paths(meson.current_source_dir(), 'test.scm'))
    configure_file(
        input: 'remote_test.hh.in',
        output: 'remote_test.hh',
        configuration: test_config
    )
    test(
        'guile/remote',
        executable(
            'guile-remote-test',
            sources: 'remote_test.cc',
            cpp_args: test_cpp_args,
            include_directories: [src],
            dependencies: [test_sbn,dtest] + valgrind_dep,
            implicit_include_directories: false,
        ),
        workdir: meson.build_root(),
        is_parallel: false
    )
endif
