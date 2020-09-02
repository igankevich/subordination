(use-modules 
 (zlib)
 (oop goops)
 (ice-9 rdelim)
 (ice-9 binary-ports)
 (ice-9 textual-ports)
 (ice-9 iconv)
 (ice-9 i18n)
 (srfi srfi-1)
 (srfi srfi-13)
 (ice-9 hash-table)
 (ice-9 ftw))

(define all-data '())

(define (process-file path-to-file)
 (alist->hash-table 
 (map (lambda (s)
  (let ((key (substring s 0 16))) 
  (let ((data-string (string-tokenize  (substring s 16 (string-length s)) (char-set-complement (char-set #\space)))))
   (cons key (map string->number data-string))
    )))
  (filter (lambda (str) (string<> str "")) (cdr (string-split (call-with-input-file path-to-file get-string-all) #\newline )))
 ))
)

(define (process-dir path statinfo flag base level)
 ;;(display flag)
 (if (eq? (stat:type statinfo) 'regular) (set! all-data (cons (process-file path) all-data )))
 #t
)

(define all-data-hash (make-hash-table))

(define (add-list-to-hash-table key value)
 (if (hash-ref all-data-hash key) 
  (hash-set! all-data-hash key (cons value (hash-ref all-data-hash key)))
  (hash-set! all-data-hash key (cons value '()))
 )
)

(define (update-all-data-hash hash-table)
 (hash-for-each add-list-to-hash-table hash-table)
)

(define (merge-table list-of-tables) 
 (map update-all-data-hash list-of-tables)
)

(define (filter-hash-table table)
    (hash-for-each (lambda (key value) 
     (if (zero? (- 5 (length value)) ) (hash-remove! table key) )) 
    table)
)

(define (make-seq-n-reverse n) 
 (if (zero? n) 
  '()
  (cons (- n 1) (make-seq-n-reverse (- n 1) )) )
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

(define (compute-variance lists) 
 (let ((theta-0 0) (theta-1 (* 2 3.1415)) (n (length (list-ref lists 0))) )
 (let (( angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) )))  (make-seq-n n)) )) ( ;; angle
   (fold spectrum 0 (list-ref lists 0) (list-ref lists 1) (list-ref lists 2) (list-ref lists 3) (list-ref lists 4) angles)
  )
 ) )
)


(nftw (list-ref (program-arguments) 1) process-dir)
(merge-table all-data)
(filter-hash-table all-data-hash)

(display (hash-count (const #t) all-data-hash))
;;(hash-for-each (lambda (key value)
;;    (display key)
;;    (display " : ")
;;    (for-each (lambda (s) (display s)(newline) ) value)
;;    (newline)
;;    (newline))
;;    all-data-hash
;;)
;;(display (make-seq-n 42))
;;(display (hash-ref all-data-hash "2010 06 29 10 00"))
(display (compute-variance (hash-ref all-data-hash "2010 06 29 10 00")))
(newline)