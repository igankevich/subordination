zlib = dependency('zlib', required: false)

if zlib.found()
    executable(
        'spec',
        sources: 'spec.cc',
        include_directories: [srcdir],
        dependencies: [threads, zlib, unistdx, subordination_app]
    )
endif
