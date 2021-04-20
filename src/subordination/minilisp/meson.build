sbn_minilisp_src = files([
    'minilisp.cc',
])

clang_tidy_files += sbn_minilisp_src

sbn_minilisp_deps = [sbn,threads,unistdx]

sbn_minilisp_exe = executable(
    'sbn-minilisp',
    sources: sbn_minilisp_src,
    dependencies: sbn_minilisp_deps,
    include_directories: src,
    install: true,
    implicit_include_directories: false,
)
