dtest_src = files([
    'application.cc',
    'cluster.cc',
    'cluster_node_bitmap.cc',
    'exit_code.cc',
])

dtest_lib_deps = [unistdx,threads]

dtest_lib = library(
    'dtest',
    sources: dtest_src,
    include_directories: src,
    dependencies: dtest_lib_deps,
    implicit_include_directories: false,
    version: meson.project_version(),
)

dtest = declare_dependency(
    link_with: dtest_lib,
    include_directories: src,
    dependencies: dtest_lib_deps,
)

dtest_exe = executable(
    'dtest',
    sources: dtest_src + ['main.cc'],
    include_directories: src,
    dependencies: dtest,
    implicit_include_directories: false,
)

clang_tidy_files += dtest_src
