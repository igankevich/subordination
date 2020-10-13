sbn_python_src = files([
    'main.cc',
    'init.cc',
])

clang_tidy_files += sbn_python_src

sbn_python_deps = [
    sbn,
    threads,
    unistdx,
    dependency('python3'),
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
)
