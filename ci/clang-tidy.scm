(use-modules
  (ice-9 popen)
  (ice-9 textual-ports)
  (ice-9 pretty-print)
  (ice-9 ftw)
  (ice-9 threads)
  (srfi srfi-1)
  (guix scripts build))

(define args-before (string-split (second (command-line)) #\|))
(define args-after (string-split (third (command-line)) #\|))
(define files (cdddr (command-line)))
(pretty-print args-before)
(pretty-print args-after)
(pretty-print files)

;; add include paths from environment variables
(define cpp-args
  (append-map
    (lambda (name)
      (define value (getenv name))
      (if value
        (fold
          (lambda (dir prev)
            (if (string=? (basename dir) "c++")
              prev
              (cons (string-append "-I" dir) prev)))
          '()
          (string-split value #\:))
        '()))
    '("CPLUS_INCLUDE_PATH" "C_INCLUDE_PATH")))

;; remove GCC C++ headers
;; https://www.mail-archive.com/bug-guix@gnu.org/msg19909.html
(setenv "CPLUS_INCLUDE_PATH"
        (string-join
          (filter
            (lambda (dir) (not (string=? (basename dir) "c++")))
            (string-split (getenv "CPLUS_INCLUDE_PATH") #\:))
          ":"))

;; add Clang built-in headers to include path
(define clang-directory
  (string-trim-both
    (with-output-to-string
      (lambda ()
        (guix-build "-e" "(@ (gnu packages llvm) clang)")))))
(ftw clang-directory
     (lambda (filename statinfo flag)
       (if (member (basename filename) '("stddef.h" "limits.h"))
         (set! cpp-args (cons* "-isystem" (dirname filename) cpp-args)))
       #t))

;; add other arguemnts via pkg-config
(define more-cpp-args
  (let* ((port (open-input-pipe "pkg-config --cflags guile-3.0"))
         (args (string-split (string-trim-both (get-string-all port)) #\space)))
    (close-pipe port)
    args))

;; execute clang-tidy
(fold
  (lambda (x prev)
    (logior x prev))
  0
  (par-map
    (lambda (file)
      (define all-args (append args-before (list file) args-after cpp-args more-cpp-args))
      (apply system* all-args))
    files))
