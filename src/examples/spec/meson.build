zlib = dependency('zlib', required: false)

if zlib.found()
    executable(
        'spec',
        sources: 'spec.cc',
        include_directories: [src],
        dependencies: [threads, zlib, unistdx, subordination_app]
    )
endif
