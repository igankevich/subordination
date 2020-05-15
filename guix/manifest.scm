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
 pre-commit
 curl
 graphviz
 documentation
 perl
 tex
 guile-xyz
 groff
 rsync
 ssh
 bash)

(define sbn-manifest
  (let ((v (getenv "SBN_MANIFEST")))
    (if v (string-split v #\,) '())))

(define (if-enabled name lst)
  (if (member name sbn-manifest) lst '()))

(packages->manifest
 (append
  (list
   (@ (gnu packages unistdx) unistdx)
   (list gcc-9 "lib")
   gcc-toolchain-9
   googletest
   pkg-config
   cmake
   ninja
   meson
   python-pre-commit
   zlib)
  (if-enabled "site" (list curl graphviz doxygen perl texlive-bin haunt
                           guile-syntax-highlight groff))
  (if-enabled "ci" (list rsync openssh coreutils bash))))
