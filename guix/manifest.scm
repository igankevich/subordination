(define sbn-manifest
  (let ((v (getenv "SBN_MANIFEST")))
    (if v (string-split v #\,) '())))

(define (if-enabled name lst)
  (if (member name sbn-manifest) lst '()))

(packages->manifest
  (append
    (list (@ (gnu packages build-tools) meson)
          (@ (gnu packages ninja) ninja)
          (@ (gnu packages cmake) cmake)
          (@ (gnu packages pkg-config) pkg-config)
          (@ (gnu packages commencement) gcc-toolchain)
          (list (@ (gnu packages gcc) gcc) "lib")
          (@ (gnu packages check) googletest)
          (@ (gnu packages unistdx) unistdx)
          (@ (gnu packages unistdx) unistdx-debug)
          (@ (gnu packages elf) elfutils)
          (@ (gnu packages compression) xz)
          (@ (gnu packages pre-commit) python-pre-commit)
          (@ (gnu packages python) python-3)
          (@ (gnu packages compression) zlib)
          (@ (gnu packages guile-zlib) guile-zlib)
          (@ (gnu packages guile) guile-3.0)
          (@ (gnu packages subordination) dtest)
          (@ (stables packages mpi) openmpi-4.0.2)
          (list (@ (gnu packages llvm) clang-10) "extra") ;; clang-tidy
          (@ (gnu packages valgrind) valgrind))
    (if-enabled "site"
      (list (@ (gnu packages documentation) doxygen)
            (@ (gnu packages tex) texlive-bin)
            (@ (gnu packages guile-xyz) haunt)
            (@ (gnu packages guile-xyz) guile-syntax-highlight)
            (@ (gnu packages groff) groff)
            (@ (gnu packages xml) libxslt)))
    (if-enabled "ci"
      (list (@ (gnu packages rsync) rsync)
            (@ (gnu packages ssh) openssh)
            (@ (gnu packages base) coreutils)
            (@ (gnu packages bash) bash)
            (@ (gnu packages package-management) guix) ;; clang-tidy runs from guix repl
            ))))

;; vim:lispwords+=if-enabled
