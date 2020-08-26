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
  (hash-set! all-data-hash key value)
 )
)

(define (update-all-data-hash hash-table)
 (hash-for-each add-list-to-hash-table hash-table)
)

(define (merge-table list-of-tables) 
 (map update-all-data-hash list-of-tables)
)

(nftw (list-ref (program-arguments) 1) process-dir)
(merge-table all-data)
(display (hash-count (const #t) all-data-hash))
(newline)