(use-modules 
 (oop goops)
 (ice-9 rdelim)
 (ice-9 binary-ports)
 (ice-9 textual-ports)
 (ice-9 iconv)
 (ice-9 i18n)
 (srfi srfi-1)
 (srfi srfi-13)
 (srfi srfi-26)
 (ice-9 hash-table)
 (ice-9 ftw))

(define (process-file path-to-file)
  (alist->hash-table 
    (map (lambda (s)
      (let ((key (substring s 0 16))) 
      (let ((data-string (string-tokenize  (substring s 16 (string-length s)) (char-set-complement (char-set #\space)))))
        (cons key (cons (string-ref (string-reverse path-to-file) 8) (map string->number data-string)))
      )))
    (filter (lambda (str) (string<> str "")) (cdr (string-split (call-with-input-file path-to-file get-string-all) #\newline )))
  ))
)

(define (profile-2 name proc data1 data2)
  (define t0 (tms:clock (times)))
  (define ret (proc data1 data2))
  (define t1 (tms:clock (times)))
  (format (current-error-port) "~a: ~as\n" name
          (exact->inexact (/ (- t1 t0) internal-time-units-per-second)))
  ret)


(define (process-dir path statinfo flag base level)
 ;;(display flag)
  (if (eq? (stat:type statinfo) 'regular) (process-file path))
  #t
)

(define (merge-table list-of-tables all-hash-table) 
  (map (lambda (hash-table) 
    (hash-for-each (lambda (key value) 
      (if (hash-ref all-hash-table key) 
        (hash-set! all-hash-table key (acons (car value) (cdr value) (hash-ref all-hash-table key)))
        (hash-set! all-hash-table key (acons (car value) (cdr value) '()))
      )
    ) hash-table)
  ) list-of-tables)
)

(define (filter-hash-table table)
  (hash-for-each (lambda (key value) 
    (if (not (zero? (- 5 (length value))) ) 
      (hash-remove! table key)
    )
  ) table)
)

(define (make-seq-n-reverse n) 
  (if (zero? n) 
    '()
    (cons (- n 1) (make-seq-n-reverse (- n 1) )) 
  )
)

(define (make-seq-n n) (reverse (make-seq-n-reverse n) ))

(define (spectrum alpha-1 alpha-2 r-1 r-2 density angle sum)
  (+ sum 
    (* density 
      (/ 1 3.14159) 
      (+ 0.5 
        (* 0.01 r-1 
          (cos (- angle alpha-1))
        ) 
        (* 0.01 r-2 
          (cos (* 2 (- angle alpha-2)))
        ) 
      )
    )
  )
)

(define (compute-variance-lists lists) 
  (let ((theta-0 0) (theta-1 (* 2 3.1415)) (n (length (list-ref lists 0))) )
  (let ((angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) )))  (make-seq-n n)) )) ;; angle
    (fold spectrum 0 (cdr (assoc #\d lists)) (cdr (assoc #\i lists)) (cdr (assoc #\j lists)) (cdr (assoc #\k lists)) (cdr (assoc #\w lists)) angles)
  ))
)

(define (compute-variance-arrays lists) 
  (let ((theta-0 0) (theta-1 (* 2 3.1415)) (n (length (list-ref lists 0))) )
  (let ((angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) )))  (make-seq-n n)) )) ;; angle
  (let ((sum 0))
    (array-for-each (lambda (d i j k w a) (set! sum (+ sum (spectrum d i j k w a 0))))
      (list->array 1 (cdr (assoc #\d lists))) 
      (list->array 1 (cdr (assoc #\i lists))) 
      (list->array 1 (cdr (assoc #\j lists))) 
      (list->array 1 (cdr (assoc #\k lists))) 
      (list->array 1 (cdr (assoc #\w lists))) 
      (list->array 1 angles)
    )
  sum)))
)

(let ((all-data-hash (make-hash-table)) (all-data '()))
  ; (define (nftw-func path statinfo flag base level) 
  ;   (if (eq? (stat:type statinfo) 'regular) (set! all-data (cons (process-file path) all-data)))
  ;   #t
  ; )
  ; (nftw (list-ref (program-arguments) 1) 
  (nftw (list-ref *global-arguments* 2) 
    (lambda (path statinfo flag base level) 
      (if (eq? (stat:type statinfo) 'regular) (set! all-data (cons ((lambda (path-to-file) (alist->hash-table 
    (map (lambda (s)
      (let ((key (substring s 0 16))) 
      (let ((data-string (string-tokenize  (substring s 16 (string-length s)) (char-set-complement (char-set #\space)))))
        (cons key (cons (string-ref (string-reverse path-to-file) 8) (map string->number data-string)))
      )))
    (filter (lambda (str) (string<> str "")) (cdr (string-split (call-with-input-file path-to-file get-string-all) #\newline )))
  ))) path) all-data)))
      #t
    )
  )

  (define (show-hash-table table) (hash-for-each (lambda (key value) (display key)) table))
  ;(merge-table all-data all-data-hash)
  ;(filter-hash-table all-data-hash)
  ;(display (compute-variance (hash-ref all-data-hash "2010 06 29 10 00")))
  (newline)
  ; (profile-2 "merge" merge-table all-data all-data-hash))
  (display (tms:clock (times)))
  (newline)
  (merge-table all-data all-data-hash)
  ; (map (lambda (table) (hash-for-each (lambda (key value) (display key)) table)) all-data)
  (display (tms:clock (times)))
  (newline)
  ; (profile "filter" (lambda () (filter-hash-table all-data-hash)))
  ; (profile "lists" (lambda () (hash-for-each (lambda (key value) (compute-variance-lists value)) all-data-hash)))
  ; (profile "arrays" (lambda () (hash-for-each (lambda (key value) (compute-variance-arrays value)) all-data-hash)))
  ; (newline)
  ; (display "equals: ")
  ; (display (= (compute-variance-lists (hash-ref all-data-hash "2010 06 29 10 00")) (compute-variance-arrays (hash-ref all-data-hash "2010 06 29 10 00"))  ))
  ; (newline)
  ; (display (compute-variance-lists (hash-ref all-data-hash "2010 06 29 10 00")))
  ; (newline)
  ; (display (compute-variance-arrays (hash-ref all-data-hash "2010 06 29 10 00")))
  ; (newline)
)