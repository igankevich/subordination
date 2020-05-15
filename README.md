# Build

    cp guix/channels.scm $HOME/.config/guix/channels.scm
    guix pull
    guix environment -m guix/manifest.scm  # dependencies
    meson build
    cd build
    ninja                                  # build
    ninja doc                              # build documentation (requires network)
    ninja test                             # run unit tests

# Run tests and benchmarks

    cd build
    ninja test
    ninja benchmark
