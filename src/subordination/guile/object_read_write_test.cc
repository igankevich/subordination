#include <gtest/gtest.h>

#include <subordination/core/kernel_buffer.hh>
#include <subordination/guile/init.hh>
#include <subordination/guile/kernel.hh>

using namespace sbn::guile;

void write_read(SCM expected) {
    sbn::kernel_buffer buffer;
    object_write(buffer, expected);
    buffer.flip();
    SCM actual = SCM_UNDEFINED;
    object_read(buffer, actual);
    EXPECT_TRUE(scm_is_true(scm_equal_p(expected, actual)))
        << "type:     " << object_to_string(scm_class_name(scm_class_of(expected)))
        << "\nexpected: " << object_to_string(expected)
        << "\nactual:   " << object_to_string(actual);
}

SCM eval(const char* s) {
    return scm_c_eval_string(s);
}

TEST(object_read_write, _) {
    write_read(eval("123"));
    write_read(eval("1.23"));
    write_read(eval("1+2i"));
    write_read(eval("#t"));
    write_read(eval("#f"));
    write_read(eval("'()"));
    write_read(eval("(cons 1 2)"));
    write_read(eval("(list 1 2 3 4)"));
    write_read(eval("'((1 2) 3 (4 5 (6)))"));
    write_read(eval("'my-symbol"));
    write_read(eval("#:my-keyword"));
    write_read(eval("\"abc\""));
    write_read(eval("#\\newline"));
    write_read(SCM_UNSPECIFIED);
    write_read(SCM_UNDEFINED);
    write_read(eval(R"(
(use-modules (oop goops))
(define-class <my-class> ()
  (field-1 #:init-keyword #:field-1 #:accessor my-class-field-1)
  (field-2 #:init-keyword #:field-2 #:accessor my-class-field-2))
(define (<my-class>-write buffer object)
  (write-object buffer (my-class-field-1 object))
  (write-object buffer (my-class-field-2 object)))
(define (<my-class>-read buffer object)
  (set! (my-class-field-1 object) (read-object buffer))
  (set! (my-class-field-2 object) (read-object buffer)))
(define-method (equal? (a <my-class>) (b <my-class>))
  (and (equal? (my-class-field-1 a) (my-class-field-1 b))
       (equal? (my-class-field-2 a) (my-class-field-2 b))))
(define-method (write (o <my-class>) port)
  (format port "[~a ~a]" (my-class-field-1 o) (my-class-field-2 o)))
(make <my-class> #:field-1 "abc" #:field-2 123)
)"));
}

void nested_main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    std::exit(RUN_ALL_TESTS());
}

int main(int argc, char* argv[]) {
    sbn::guile::main(argc, argv, nested_main);
    return 0;
}
