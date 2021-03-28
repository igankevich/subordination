sbn_guile_src = files([
    'base.cc',
    'init.cc',
    'kernel.cc',
    'main.cc',
    'expression_kernel.cc',
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
