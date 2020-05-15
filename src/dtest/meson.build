dtest_src = files([
    'application.cc',
    'cluster.cc',
    'cluster_node_bitmap.cc',
    'dtest_main.cc',
    'exit_code.cc',
])

dtest_deps = [unistdx,threads]

dtest_exe = executable(
    'dtest',
    sources: dtest_src,
    include_directories: src,
    dependencies: dtest_deps,
)
