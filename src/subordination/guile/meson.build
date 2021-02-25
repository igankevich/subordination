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
        'guile-local-test',
        sources: ['guile_local_test.cc'],
        dependencies: [unistdx] + valgrind_dep,
        include_directories: src,
        implicit_include_directories: false,
    ),
    args: [sbn_guile_exe, join_paths(meson.current_source_dir(), 'test.scm')],
    env: test_env,
)
