autoreg_src = [
    'acf_generator.cc',
    'autoreg.cc',
    'autoreg_driver.cc',
    'main.cc',
    'ssysv.cc',
    'variance_wn.cc',
    'yule_walker.cc'
]

autoreg_deps = [threads, unistdx, sbn]
autoreg_cpp_args = ['-DAUTOREG_REAL_TYPE=double'] + (with_debug ? ['-DAUTOREG_DEBUG'] : [])

executable(
    'autoreg',
    sources: autoreg_src,
    include_directories: [src,examples],
    dependencies: autoreg_deps,
    cpp_args: autoreg_cpp_args,
    implicit_include_directories: false,
    install: true,
)

openmpi = dependency('mpi', language: 'cpp')

executable(
    'autoreg-mpi',
    sources: autoreg_src + ['mpi.cc'],
    include_directories: [src,examples],
    dependencies: autoreg_deps + [openmpi],
    cpp_args: autoreg_cpp_args + ['-DAUTOREG_MPI'],
    implicit_include_directories: false,
    install: true,
)
