(use-package-modules
  base
  build-tools
  check
  cmake
  commencement
  compression
  gcc
  ninja
  pkg-config
  unistdx
  documentation
  curl
  graphviz
  pre-commit)

(packages->manifest
  (list
    unistdx-0.18.0
    (list gcc-9 "lib")
    gcc-toolchain-9
    googletest
    pkg-config
    cmake
    ninja
    meson
    python-pre-commit
    zlib
    ;; documentation
    curl graphviz doxygen))
