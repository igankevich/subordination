(use-modules
  (srfi srfi-1)
  (ice-9 format)
  (ice-9 ftw)
  (ice-9 regex)
  (ice-9 textual-ports)
  (ice-9 pretty-print)
  (oop goops))

(display "application started\n" (current-error-port))
(force-output (current-error-port))

(if *first-node*
  (let ()
    (kernel-map
      '(lambda (number)
         (display (format #f "number ~a\n" number) (current-error-port))
         (force-output (current-error-port))
         (* number 2))
      (iota 10)
      #:child-pipeline 'remote)
    (kernel-react
      '(lambda (new-rows rows)
         (append new-rows rows))
      '(lambda (rows)
         (define actual (sort (map car rows) (lambda (a b) (< a b))))
         (define expected (map (lambda (x) (* x 2)) (iota 10)))
         (format (current-error-port) "postamble\n")
         (format (current-error-port) "Actual:   ~a\n" actual)
         (format (current-error-port) "Expected: ~a\n" expected)
         (force-output (current-error-port))
         (if (not (equal? expected actual))
           (exit 1)))
      '())))
