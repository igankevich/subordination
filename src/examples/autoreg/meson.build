benchmark(
    'autoreg',
    executable(
        'autoreg',
        sources: ['main.cc', 'autoreg.cc', 'autoreg_driver.cc', 'ssysv.cc', 'acf_generator.cc',
                  'variance_wn.cc', 'yule_walker.cc'],
        include_directories: [src,examples],
        dependencies: [threads, unistdx, sbn],
        cpp_args: ['-DAUTOREG_REAL_TYPE=double'] + (with_debug ? ['-DAUTOREG_DEBUG'] : []),
        implicit_include_directories: false,
    )
)
