(use-modules
  (srfi srfi-1)
  (ice-9 format)
  (ice-9 ftw)
  (ice-9 regex)
  (ice-9 textual-ports)
  (ice-9 pretty-print)
  (oop goops))

(define filename-rx (make-regexp "^([0-9]+)([dijkw])([0-9]+)\\.txt\\.gz$"))
(define not-space (char-set-complement (char-set #\space)))
(define %pi (acos -1))

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

(define (spectrum alpha-1 alpha-2 r-1 r-2 density angle sum)
  (+ sum
     (* density
        (/ 1 %pi)
        (+ 0.5
           (* 0.01 r-1 (cos (- angle alpha-1)))
           (* 0.01 r-2 (cos (* 2 (- angle alpha-2))))))))

(define (compute-variance arrays)
  (define alpha-1 (list-ref arrays 0))
  (define alpha-2 (list-ref arrays 1))
  (define r-1 (list-ref arrays 2))
  (define r-2 (list-ref arrays 3))
  (define density (list-ref arrays 4))
  (let* ((theta-0 0)
         (theta-1 (* 2 %pi))
         (n (length alpha-1))
         (angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) ))) (iota n))))
    (fold spectrum 0 alpha-1 alpha-2 r-1 r-2 density angles)))

(define input-directories (cdr *application-arguments*))
(define files (make-hash-table))
(format (current-error-port) "input-directories ~a\n" input-directories)
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
(hash-for-each
  (lambda (key value)
    (format (current-error-port) "~a length ~a\n" key (length value)))
  files)
(kernel-map
  '(lambda (five-files)
     (format (current-error-port) "five-files ~a\n" five-files)
     (define records (make-hash-table))
     ;; read five files
     (for-each
       (lambda (file)
         (define variable (spectrum-file-variable file))
         (define lines
           (string-split
             (string-trim-both (gzip-file->string (spectrum-file-path file)))
             #\newline))
         (for-each
           (lambda (s)
             (let* ((timestamp (substring s 0 16))
                    (data-string (string-tokenize (substring s 16) not-space))
                    (new-array (map string->number data-string))
                    (arrays (hash-ref records timestamp)))
               (if (not arrays) (set! arrays (make-list 5)))
               (list-set! arrays (variable->index variable) new-array)
               (hash-set! records timestamp arrays)))
           (cdr lines))
         (format (current-error-port) "~a: ~a records\n"
                 (spectrum-file-path file)
                 (hash-count (const #t) records)))
       five-files)
     ;; remove incomplete records
     (hash-for-each
       (lambda (timestamp arrays)
         (if (not (= (length (delete-duplicates (map length arrays))) 1))
           (begin
             (format (current-error-port) "remove ~a length ~a\n"
                     timestamp (map length arrays))
             (force-output (current-error-port))
             (hash-remove! records timestamp))))
       records)
     ;; compute variance
     (map
       (lambda (data)
         (define file (car five-files))
         (cons (spectrum-file-station file) data))
       (hash-map->list
         (lambda (timestamp arrays)
           (list timestamp (compute-variance arrays)))
         records)))
  (hash-map->list (lambda (key value) value) files))

(kernel-react
  '(lambda (new-row rows)
     (cons new-row rows))
  '(lambda (rows)
     (pretty-print rows)
     (force-output (current-error-port)))
  '())
