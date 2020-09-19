zlib = dependency('zlib', required: false)

if zlib.found()
    executable(
        'spec',
        sources: ['main.cc','timestamp.cc','variable.cc'],
        include_directories: [src,examples],
        dependencies: [threads, zlib, unistdx, sbn],
        implicit_include_directories: false,
        cpp_args: ['-DSPECTRUM_REAL_TYPE=double'],
        install: true,
    )
endif
