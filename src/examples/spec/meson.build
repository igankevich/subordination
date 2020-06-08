zlib = dependency('zlib', required: false)

if zlib.found()
    executable(
        'spec',
        sources: 'spec.cc',
        include_directories: [src],
        dependencies: [threads, zlib, unistdx, sbn],
        implicit_include_directories: false,
    )
endif
