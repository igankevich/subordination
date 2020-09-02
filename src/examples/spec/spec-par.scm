(make-kernel
  #:data 0
  #:act '(begin
           (use-modules (ice-9 format))
           (format (current-error-port) "parent kernel act\n")
           (force-output (current-error-port))
           (for-each
             (lambda (i)
               (upstream *self*
                         (make-kernel
                           #:act '(begin
                                    (use-modules (ice-9 format))
                                    (format (current-error-port) "child kernel act\n")
                                    (force-output (current-error-port))
                                    (commit *self*)))))
             (iota 2)))
  #:react '(begin
             (variable-set! *data* (+ (variable-ref *data*) 1))
             (format (current-error-port) "parent kernel react: data=~a\n"
                     (kernel-data *self*))
             (force-output (current-error-port))
             (if (= (variable-ref *data*) 2)
               (commit *self* 0 'local))))
