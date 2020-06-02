(define sbn-manifest
  (let ((v (getenv "SBN_MANIFEST")))
    (if v (string-split v #\,) '())))

(define (if-enabled name lst)
  (if (member name sbn-manifest) lst '()))

(packages->manifest
 (append
  (list
   (@ (gnu packages build-tools) meson)
   (@ (gnu packages ninja) ninja)
   (@ (gnu packages cmake) cmake)
   (@ (gnu packages pkg-config) pkg-config)
   (@ (gnu packages commencement) gcc-toolchain)
   (list (@ (gnu packages gcc) gcc) "lib")
   (@ (gnu packages check) googletest)
   (@ (gnu packages unistdx) unistdx)
   (@ (gnu packages pre-commit) python-pre-commit)
   (@ (gnu packages python-xyz) python-chardet)
   (@ (gnu packages compression) zlib))
  (if-enabled "site"
              (list
               (@ (gnu packages documentation) doxygen)
               (@ (gnu packages tex) texlive-bin)
               (@ (gnu packages guile) guile-2.2)
               (@ (gnu packages guile-xyz) haunt)
               (@ (gnu packages guile-xyz) guile-syntax-highlight)
               (@ (gnu packages groff) groff)
               (@ (gnu packages xml) libxslt)))
  (if-enabled "ci"
              (list
               (@ (gnu packages rsync) rsync)
               (@ (gnu packages ssh) openssh)
               (@ (gnu packages base) coreutils)
               (@ (gnu packages bash) bash)))))
