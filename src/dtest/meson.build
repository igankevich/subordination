dtest_src = files([
    'application.cc',
    'cluster.cc',
    'cluster_node_bitmap.cc',
    'exit_code.cc',
    'main.cc',
])

dtest_deps = [unistdx,threads]

dtest_exe = executable(
    'dtest',
    sources: dtest_src,
    include_directories: src,
    dependencies: dtest_deps,
    implicit_include_directories: false,
)
