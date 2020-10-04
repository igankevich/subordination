hello_world_src = files(['main.cc'])
hello_world_deps = [threads, unistdx, sbn]
clang_tidy_files += hello_world_src
executable(
    'hello-world',
    sources: hello_world_src,
    include_directories: [src,examples],
    dependencies: hello_world_deps,
    implicit_include_directories: false,
    install: true,
)
