(use-modules
 (oop goops)
 (ice-9 rdelim)
 (ice-9 binary-ports)
 (ice-9 textual-ports)
 (ice-9 iconv)
 (ice-9 i18n)
 (ice-9 threads)
 (srfi srfi-1)
 (srfi srfi-13)
 (srfi srfi-26)
 (ice-9 hash-table)
 (ice-9 ftw))


(define (my-map proc lst)
    (if (null? lst) '()
        (cons (proc (car lst)) (my-map proc (cdr lst)))))

(define (do-fold-pairwise proc lst)
  (if (null? lst)
    '()
    (if (null? (cdr lst))
      lst
      (do-fold-pairwise proc
                     (cons
                       (proc (car lst) (car (cdr lst)))
                       (do-fold-pairwise proc (cdr (cdr lst))))))))
(define (fold-pairwise proc lst)
  (car (do-fold-pairwise proc lst)))

;   (define (spectrum alpha-1 alpha-2 r-1 r-2 density angle sum)
;   (+ sum 
;     (* density 
;       (/ 1 3.14159) 
;       (+ 0.5 
;         (* 0.01 r-1 
;           (cos (- angle alpha-1))
;         ) 
;         (* 0.01 r-2 
;           (cos (* 2 (- angle alpha-2)))
;         ) 
;       )
;     )
;   )
; )
; обернул в лямбду, чтобы работало в одном потоке
(define spectrum (lambda (alpha-1 alpha-2 r-1 r-2 density angle sum)
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
))

(define (compute-variance-lists lists) 
  (let ((theta-0 0) (theta-1 (* 2 3.1415)) (n (length (list-ref lists 0))) )
  (let ((angles (map (lambda (j) (+ theta-0 (* (- theta-1 theta-0) (/ j n) )))  (make-seq-n n)) )) ;; angle
    (fold-pairwise spectrum 0 (cdr (assoc #\d lists)) (cdr (assoc #\i lists)) (cdr (assoc #\j lists)) (cdr (assoc #\k lists)) (cdr (assoc #\w lists)) angles)
  ))
)

(let ((all-data-hash (make-hash-table))
      (all-data '())
      (mutex (make-mutex)))
;  (nftw (list-ref (program-arguments) 1)
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
;  all-data -- list of hash tables
  (my-map (lambda (hash-table)
    (hash-for-each (lambda (key value)
      (lock-mutex mutex)
      (if (hash-ref all-data-hash key)
        (hash-set! all-data-hash key (acons (car value) (cdr value) (hash-ref all-data-hash key)))
        (hash-set! all-data-hash key (acons (car value) (cdr value) '()))
      )
      (unlock-mutex mutex)
    ) hash-table)
  ) all-data)

;   (hash-for-each (lambda (k v) (display k)) all-data-hash)


    (my-map (lambda (pair) (if (= 5 (length (cadr pair))) (compute-variance-lists (cadr pair)))) 
    
      (hash-map->list (lambda (k v) (cons k v)) all-data-hash)
    )
    ; (my-map (lambda (table) (hash-map->list (lambda (k v) (cons k v)) table)) all-data)

;   (hash-for-each (lambda (key value)
;     (if (not (zero? (- 5 (length value))) )
;       (hash-remove! table key)
;     )
;   ) all-data-hash)
)
