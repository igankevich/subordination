zlib = dependency('zlib')
spec_src = files(['main.cc','timestamp.cc','variable.cc'])
spec_deps = [threads, zlib, unistdx, sbn]
clang_tidy_files += spec_src
executable(
    'spec',
    sources: spec_src,
    include_directories: [src,examples],
    dependencies: spec_deps,
    implicit_include_directories: false,
    cpp_args: ['-DSPECTRUM_REAL_TYPE=double'],
    install: true,
)
