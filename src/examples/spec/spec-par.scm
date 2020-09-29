(use-modules
  (srfi srfi-1)
  (ice-9 format)
  (ice-9 ftw)
  (ice-9 regex)
  (ice-9 textual-ports)
  (oop goops))

(define filename-rx (make-regexp "^([0-9]+)([dijkw])([0-9]+)\\.txt\\.gz$"))

(define (variable->index var)
  (cond
    ((string=? var "d") 0)
    ((string=? var "i") 1)
    ((string=? var "j") 2)
    ((string=? var "k") 3)
    ((string=? var "w") 4)
    (else (throw 'sbn-error "unknown spectrum variable" var))))

(define-class <spectrum-file> ()
  (path #:init-keyword #:path #:accessor spectrum-file-path)
  (station #:init-keyword #:station #:accessor spectrum-file-station)
  (year #:init-keyword #:year #:accessor spectrum-file-year)
  (variable #:init-keyword #:variable #:accessor spectrum-file-variable))

(define-generic object->list)

(define-method (object->list (o <spectrum-file>))
  (list '<spectrum-file>
        (spectrum-file-path o)
        (spectrum-file-station o)
        (spectrum-file-year o)
        (spectrum-file-variable o)))

(define (object-read lst)
  (cond
    ((null? lst) '())
    ((eq? (car lst) '<list>)
     (alist->hash-table lst))
    ((eq? (car lst) '<hashtable>)
     (alist->hash-table lst))
    ((eq? (car lst) '<spectrum-file>)
     (make-hash-table
       #:path (list-ref lst 1)
       #:station (list-ref lst 2)
       #:year (list-ref lst 3)
       #:variable (list-ref lst 4)))
    (else lst)))

(define-method (object->list object)
  (cond
    ((list? object) (cons '<list> object) out)
    (else object)))

(define-method (object->list (table <hashtable>))
  (hash-map->list cons table))

(define (make-file-kernel file)
  (make-kernel
    #:data (list (cons 'file file)
                 (cons 'records (make-hash-table)))
    #:act '(begin
             (define file (assq-ref (variable-ref *data*) 'file))
             (define lines
               (string-split
                 (string-trim-both (gzip-file->string (spectrum-file-path file)))
                 ;;(call-with-input-file (spectrum-file-path file) get-string-all)
                 #\newline))
             (define records (assq-ref (variable-ref *data*) 'records))
             (define not-space (char-set-complement (char-set #\space)))
             (for-each
               (lambda (s)
                 ;;(format (current-error-port) "line ~a\n" (string-trim-both s))
                 ;;(force-output (current-error-port))
                 (let* ((key (substring s 0 16))
                        (data-string (string-tokenize (substring s 16) not-space))
                        (values (map string->number data-string)))
                   (hash-set! records key values)))
               (cdr lines))
             (format (current-error-port) "~a: ~a records\n"
                     (spectrum-file-path file)
                     (hash-count (const #t) records))
             (commit *self*))))

(define (make-five-files-kernel files)
  (make-kernel
    #:data (list (cons 'files files)
                 (cons 'num-kernels 5)
                 (cons 'records (make-hash-table)))
    #:act '(begin
             (define files (assq-ref (variable-ref *data*) 'files))
             (assq-set! (variable-ref *data*) 'num-kernels (length files))
             (for-each
               (lambda (file)
                 (upstream *self*
                           (make-file-kernel file)))
               files))
    #:react '(begin
               (define child-data (variable-ref (kernel-data *child*)))
               (define variable (spectrum-file-variable (assq-ref child-data 'file)))
               (define records (assq-ref (variable-ref *data*) 'records))
               (assq-set! (variable-ref *data*) 'num-kernels
                          (- (assq-ref (variable-ref *data*) 'num-kernels) 1))
               (hash-for-each
                 (lambda (timestamp new-array)
                   (define arrays (hash-ref records timestamp))
                   (if (not arrays) (set! arrays (make-list 5)))
                   (list-set! arrays (variable->index variable) new-array)
                   (hash-set! records timestamp arrays))
                 (assq-ref child-data 'records))
               (format (current-error-port) "five-files-kernel react: num-kernels ~a records ~a\n"
                       (assq-ref (variable-ref *data*) 'num-kernels)
                       child-data)
               (force-output (current-error-port))
               (if (= (assq-ref (variable-ref *data*) 'num-kernels) 0)
                 (begin
                   (format (current-error-port) "five-files-kernel react: records ~a\n"
                           (assq-ref (variable-ref *data*) 'records))
                   (force-output (current-error-port))
                   (hash-for-each
                     (lambda (timestamp arrays)
                       (if (not (= (length (delete-duplicates (map length arrays))) 1))
                         (begin
                           (format (current-error-port) "five-files-kernel react: remove ~a length ~a\n"
                                   timestamp (map length arrays))
                           (force-output (current-error-port))
                           (hash-remove! records timestamp))))
                     records)
                   (hash-for-each
                     (lambda (timestamp arrays)
                       )
                     records)
                   (commit *self*))))))

(define (make-main-kernel)
  (make-kernel
    #:data `((num-kernels . 0))
    #:act '(begin
             (define input-directories (cdr (kernel-application-arguments *self*)))
             (define files (make-hash-table))
             (format (current-error-port) "input-directories ~a\n"
                     input-directories)
             (format (current-error-port) "parent kernel act\n")
             (force-output (current-error-port))
             (for-each
               (lambda (dir)
                 (nftw dir
                       (lambda (path statinfo flag base level)
                         (if (and (eq? flag 'regular)
                                  (eq? (stat:type statinfo) 'regular))
                           (let* ((filename (basename path))
                                  (match (regexp-exec filename-rx filename)))
                             (if match
                               (let* ((station (match:substring match 1))
                                      (variable (match:substring match 2))
                                      (year (match:substring match 3))
                                      (key (string-append year "/" station))
                                      (existing-value (hash-ref files key)))
                                 (hash-set! files key
                                            (cons (make <spectrum-file>
                                                    #:path path
                                                    #:station station
                                                    #:year year
                                                    #:variable variable)
                                                  (if existing-value existing-value '())))))))
                         #t)
                       'mount 'physical))
               input-directories)
             (hash-for-each
               (lambda (key value)
                 (if (not (= (length value) 5))
                   (begin
                     (format (current-error-port) "remove ~a length ~a\n" key (length value))
                     (hash-remove! files key))))
               files)
             (assq-set! (variable-ref *data*) 'num-kernels (hash-count (const #t) files))
             (format (current-error-port) "num-kernels ~a\n" (assq-ref (variable-ref *data*) 'num-kernels))
             (hash-for-each
               (lambda (key value)
                 (upstream *self* (make-five-files-kernel value)))
               files))
    #:react '(begin
               (assq-set! (variable-ref *data*) 'num-kernels
                          (- (assq-ref (variable-ref *data*) 'num-kernels) 1))
               (format (current-error-port) "parent kernel react: num-kernels ~a\n"
                       (assq-ref (variable-ref *data*) 'num-kernels))
               (force-output (current-error-port))
               (if (= (assq-ref (variable-ref *data*) 'num-kernels) 0)
                 (commit *self*)))))

(make-main-kernel)
