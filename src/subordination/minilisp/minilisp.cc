// This software is in the public domain.
// https://github.com/rui314/minilisp/blob/nogc/minilisp.c

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sstream>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel.hh>

template <class ... Args>
inline void
log(const Args& ... args) {
    sys::log_message("lisp", args...);
}

//======================================================================
// Lisp objects
//======================================================================

// The Lisp object type
enum Type {
    // Regular objects visible from the user
    TINT = 1,
    TCELL,
    TSYMBOL,
    TPRIMITIVE,
    TFUNCTION,
    TMACRO,
    TSPECIAL,
    TENV,
};

const char* type_to_string(Type t) noexcept {
    switch (t) {
        case TINT: return "integer";
        case TCELL: return "cell";
        case TSYMBOL: return "symbol";
        case TPRIMITIVE: return "primitive";
        case TFUNCTION: return "function";
        case TMACRO: return "macro";
        case TSPECIAL: return "special";
        case TENV: return "environment";
        default: return nullptr;
    }
}

std::ostream& operator<<(std::ostream& out, Type rhs) {
    auto s = type_to_string(rhs);
    return out << (s ? s : "unknown");
}

// Subtypes for TSPECIAL
enum {
    TNIL = 1,
    TDOT,
    TCPAREN,
    TTRUE,
};

struct Obj;
class Kernel;

// Typedef for the primitive function.
typedef struct Obj* Primitive(Kernel* kernel, struct Obj* env, struct Obj* args);

// The object type
typedef struct Obj {
    // The first word of the object represents the type of the object. Any code that handles object
    // needs to check its type first, then access the following union members.
    Type type;

    // Object values.
    union {
        // Int
        int value;
        // Cell
        struct {
            struct Obj* car;
            struct Obj* cdr;
        };
        // Symbol
        char name[1];
        // Primitive
        Primitive *fn;
        // Function or Macro
        struct {
            struct Obj* params;
            struct Obj* body;
            struct Obj* env;
        };
        // Subtype for special type
        int subtype;
        // Environment frame. This is a linked list of association lists
        // containing the mapping from symbols to their value.
        struct {
            struct Obj* vars;
            struct Obj* up;
        };
        // Forwarding pointer
        void *moved;
    };
} Obj;

static Obj* eval(Kernel* kernel, Obj* env, Obj* obj);
static Obj* cons(Obj* car, Obj* cdr);
static Obj* eval_list(Kernel* kernel, Obj* env, Obj* list);
static Obj* push_env(Obj* env, Obj* vars, Obj* values);
static Obj* progn(Kernel* kernel, Obj* env, Obj* list);
static size_t list_length(Obj* list);

// Constants
static Obj* Nil;
static Obj* Dot;
static Obj* Cparen;
static Obj* True;

Obj* vector_to_list(const std::vector<Obj*>& vec) {
    const auto n = vec.size();
    if (n == 0) { return Nil; }
    Obj* list = Nil;
    for (size_t i=0; i<n; ++i) {
        size_t j = n-1-i;
        list = cons(vec[j], list);
    }
    return list;
}

const char* kernel_name(sbn::kernel* k) {
    return k ? typeid(*k).name() : "null";
}

std::string kernel_stack(sbn::kernel* k) {
    std::string s;
    for (; k; k=k->parent()) {
        s = typeid(*k).name() + std::string(" ") + s;
    }
    return s;
}

class Kernel: public sbn::kernel {

private:
    Obj* _environment{};
    Obj* _expression{};
    Obj* _result{};
    size_t _index = 0;

public:
    Kernel() = default;
    inline Kernel(Obj* env, Obj* expr): Kernel(env, expr, 0) {}
    inline Kernel(Obj* env, Obj* expr, size_t index):
    _environment(env), _expression(expr), _index(index) {}

    void act() override {
        this->_result = eval(this, this->_environment, this->_expression);
        if (this->_result) { sbn::commit(std::move(this_ptr())); }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result());
        sbn::commit(std::move(this_ptr()));
    }

    inline Obj* result() noexcept { return this->_result; }
    inline const Obj* result() const noexcept { return this->_result; }
    inline void result(Obj* rhs) noexcept { this->_result = rhs; }
    inline Obj* environment() noexcept { return this->_environment; }
    inline const Obj* environment() const noexcept { return this->_environment; }
    inline Obj* expression() noexcept { return this->_expression; }
    inline const Obj* expression() const noexcept { return this->_expression; }
    inline void expression(Obj* rhs) noexcept { this->_expression = rhs; }
    inline size_t index() const noexcept { return this->_index; }
    inline void index(size_t rhs) noexcept { this->_index = rhs; }

};

class Cons: public Kernel {

private:
    Obj* _a{};
    Obj* _b{};
    Obj* _result_a{};
    Obj* _result_b{};
    std::atomic<int> _count{0};

public:
    Cons() = default;
    inline Cons(Obj* env, Obj* expr): Kernel(env, expr),
    _a(expr->car), _b(expr->cdr->car) {}

    void act() override {
        this->_result_a = eval(this, environment(), this->_a);
        this->_result_b = eval(this, environment(), this->_b);
        if (this->_result_a) { ++this->_count; }
        if (this->_result_b) { ++this->_count; }
        if (this->_count == 2) {
            result(cons(this->_result_a, this->_result_b));
            log("< cons _ _ > _", this->_a, this->_b, result());
            sbn::commit(std::move(this_ptr()));
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        if (child->expression() == this->_a) {
            this->_result_a = child->result();
        }
        if (child->expression() == this->_b) {
            this->_result_b = child->result();
        }
        ++this->_count;
        if (this->_count == 2) {
            result(cons(this->_result_a, this->_result_b));
            log("< cons _ _ > _", this->_a, this->_b, result());
            sbn::commit(std::move(this_ptr()));
        }
    }

};

class Progn: public Kernel {
public:
    Progn() = default;
    inline Progn(Obj* env, Obj* expr): Kernel(env, expr) {}

    void act() override {
        auto list = expression();
        while (list != Nil) {
            auto element = list->car;
            list = list->cdr;
            expression(list);
            result(eval(this, environment(), element));
            if (!result()) {
                break;
            }
        }
        if (result()) {
            sbn::commit(std::move(this_ptr()));
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result());
        log("Xeval _> _", kernel_stack(this), result());
        //auto list = expression();
        //list = list->cdr;
        act();
    }

};

class Apply: public Kernel {
private:
    Obj* _new_expr{};
    std::vector<Obj*> _args;
    enum class State {Args,Progn} _state = State::Args;
public:
    Apply() = default;
    inline Apply(Obj* env, Obj* expr, Obj* new_expr): Kernel(env, expr), _new_expr(new_expr) {}

    void act() override {
        auto args = this->_new_expr->cdr;
        auto n = list_length(args);
        this->_args.resize(n);
        size_t i = 0;
        bool async = false;
        for (Obj* lp = args; lp != Nil; lp = lp->cdr, ++i) {
            auto result = eval(this, environment(), lp->car);
            if (result) {
                this->_args[i] = result;
            } else {
                async = true;
            }
        }
        if (!async) {
            this->_state = State::Progn;
            postamble();
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        if (this->_state == State::Args) {
            const auto n = this->_args.size();
            bool finished = true;
            for (size_t i=0; i<n; ++i) {
                if (this->_args[i] == child->expression()) {
                    this->_args[i] = child->result();
                }
                if (!this->_args[i]) {
                    finished = false;
                }
            }
            if (finished) {
                postamble();
            }
        } else {
            result(child->result());
            log("< apply _ > _", expression(), result());
            sbn::commit(std::move(this_ptr()));
        }
    }

private:
    void postamble() {
        auto fn = this->_new_expr->car;
        Obj* body = fn->body;
        Obj* params = fn->params;
        Obj* eargs = vector_to_list(this->_args);
        Obj* newenv = push_env(fn->env, params, eargs);
        //log("apply _ to _", fn, eargs);
        result(progn(this, newenv, body));
        if (result()) {
            log("_ < xapply _ > _", kernel_stack(this), expression(), result());
            sbn::commit(std::move(this_ptr()));
        }
    }

};

// The list containing all symbols. Such data structure is traditionally called the "obarray", but I
// avoid using it as a variable name as this is not an array but a list.
static Obj* Symbols;

static void error(const char* fmt, ...) __attribute((noreturn));

//======================================================================
// Constructors
//======================================================================

static Obj* alloc(Type type, size_t size) {
    // Add the size of the type tag.
    size += offsetof(Obj, value);

    // Allocate the object.
    Obj* obj = reinterpret_cast<Obj*>(malloc(size));
    obj->type = type;
    return obj;
}

static Obj* make_int(int value) {
    Obj* r = alloc(TINT, sizeof(int));
    r->value = value;
    return r;
}

static Obj* make_symbol(const char* name) {
    Obj* sym = alloc(TSYMBOL, strlen(name) + 1);
    strcpy(sym->name, name);
    return sym;
}

static Obj* make_primitive(Primitive *fn) {
    Obj* r = alloc(TPRIMITIVE, sizeof(Primitive *));
    r->fn = fn;
    return r;
}

static Obj* make_function(Type type, Obj* params, Obj* body, Obj* env) {
    assert(type == TFUNCTION || type == TMACRO);
    Obj* r = alloc(type, sizeof(Obj* ) * 3);
    r->params = params;
    r->body = body;
    r->env = env;
    return r;
}

static Obj* make_special(int subtype) {
    Obj* r = reinterpret_cast<Obj*>(malloc(sizeof(void *) * 2));
    r->type = TSPECIAL;
    r->subtype = subtype;
    return r;
}

struct Obj* make_env(Obj* vars, Obj* up) {
    Obj* r = alloc(TENV, sizeof(Obj* ) * 2);
    r->vars = vars;
    r->up = up;
    return r;
}

static Obj* cons(Obj* car, Obj* cdr) {
    Obj* cell = alloc(TCELL, sizeof(Obj* ) * 2);
    cell->car = car;
    cell->cdr = cdr;
    return cell;
}

// Returns ((x . y) . a)
static Obj* acons(Obj* x, Obj* y, Obj* a) {
    return cons(cons(x, y), a);
}

//======================================================================
// Parser
//
// This is a hand-written recursive-descendent parser.
//======================================================================

static Obj* read();

static void error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static int peek() {
    int c = getchar();
    ungetc(c, stdin);
    return c;
}

// Skips the input until newline is found. Newline is one of \r, \r\n or \n.
static void skip_line() {
    for (;;) {
        int c = getchar();
        if (c == EOF || c == '\n')
            return;
        if (c == '\r') {
            if (peek() == '\n')
                getchar();
            return;
        }
    }
}

// Reads a list. Note that '(' has already been read.
static Obj* read_list() {
    Obj* obj = read();
    if (!obj)
        error("Unclosed parenthesis");
    if (obj == Dot)
        error("Stray dot");
    if (obj == Cparen)
        return Nil;
    Obj* head, *tail;
    head = tail = cons(obj, Nil);

    for (;;) {
        Obj* obj = read();
        if (!obj)
            error("Unclosed parenthesis");
        if (obj == Cparen)
            return head;
        if (obj == Dot) {
            tail->cdr = read();
            if (read() != Cparen)
                error("Closed parenthesis expected after dot");
            return head;
        }
        tail->cdr = cons(obj, Nil);
        tail = tail->cdr;
    }
}

// May create a new symbol. If there's a symbol with the same name, it will not create a new symbol
// but return the existing one.
static Obj* intern(const char* name) {
    for (Obj* p = Symbols; p != Nil; p = p->cdr)
        if (strcmp(name, p->car->name) == 0)
            return p->car;
    Obj* sym = make_symbol(name);
    Symbols = cons(sym, Symbols);
    return sym;
}

// Reader marcro ' (single quote). It reads an expression and returns (quote <expr>).
static Obj* read_quote() {
    Obj* sym = intern("quote");
    return cons(sym, cons(read(), Nil));
}

static int read_number(int val) {
    while (isdigit(peek()))
        val = val * 10 + (getchar() - '0');
    return val;
}

#define SYMBOL_MAX_LEN 200

static Obj* read_symbol(char c) {
    char buf[SYMBOL_MAX_LEN + 1];
    int len = 1;
    buf[0] = c;
    while (isalnum(peek()) || peek() == '-') {
        if (SYMBOL_MAX_LEN <= len)
            error("Symbol name too long");
        buf[len++] = getchar();
    }
    buf[len] = '\0';
    return intern(buf);
}

static Obj* read() {
    for (;;) {
        int c = getchar();
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t')
            continue;
        if (c == EOF)
            return nullptr;
        if (c == ';') {
            skip_line();
            continue;
        }
        if (c == '(')
            return read_list();
        if (c == ')')
            return Cparen;
        if (c == '.')
            return Dot;
        if (c == '\'')
            return read_quote();
        if (isdigit(c))
            return make_int(read_number(c - '0'));
        if (c == '-')
            return make_int(-read_number(0));
        if (isalpha(c) || strchr("+=!@#$%^&*", c))
            return read_symbol(c);
        error("Don't know how to handle %c", c);
    }
}

std::ostream& operator<<(std::ostream& out, const Obj* obj) {
    if (!obj) {
        out << "nullptr";
        return out;
    }
    switch (obj->type) {
        case TINT:
            out << obj->value;
            break;
        case TCELL:
            out << '(';
            for (;;) {
                out << obj->car;
                if (obj->cdr == Nil)
                    break;
                if (!obj->cdr) {
                    out << " . nullptr";
                    break;
                }
                if (obj->cdr->type != TCELL) {
                    out << " . " << obj->cdr;
                    break;
                }
                out << ' ';
                obj = obj->cdr;
            }
            out << ')';
            break;
        case TSYMBOL:
            out << obj->name;
            break;
        case TPRIMITIVE:
            out << "<primitive>";
            break;
        case TFUNCTION:
            out << "<function>";
            break;
        case TMACRO:
            out << "<macro>";
            break;
        case TSPECIAL:
            if (obj == Nil) {
                out << "()";
            } else if (obj == True) {
                out << "#t";
            } else {
                error("Bug: print: Unknown subtype: %d", obj->subtype);
            }
            break;
        default:
            error("Bug: print: Unknown tag type: %d", obj->type);
    }
    return out;
}

std::string object_to_string(const Obj* obj) {
    std::stringstream tmp;
    tmp << obj;
    return tmp.str();
}

static size_t list_length(Obj* list) {
    size_t len = 0;
    for (;;) {
        if (list == Nil)
            return len;
        if (list->type != TCELL)
            error("length: cannot handle dotted list");
        list = list->cdr;
        ++len;
    }
    return len;
}

//======================================================================
// Evaluator
//======================================================================

static void add_variable(Obj* env, Obj* sym, Obj* val) {
    env->vars = acons(sym, val, env->vars);
}

// Returns a newly created environment frame.
static Obj* push_env(Obj* env, Obj* vars, Obj* values) {
    if (list_length(vars) != list_length(values))
        error("Cannot apply function: number of argument does not match");
    Obj* map = Nil;
    for (Obj* p = vars, *q = values; p != Nil; p = p->cdr, q = q->cdr) {
        Obj* sym = p->car;
        Obj* val = q->car;
        map = acons(sym, val, map);
    }
    return make_env(map, env);
}

// Evaluates the list elements from head and returns the last return value.
static Obj* progn(Kernel* kernel, Obj* env, Obj* list) {
    if (list == Nil) { return Nil; }
    if (list->cdr == Nil) { return eval(kernel, env, list->car); }
    sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Progn>(env, list));
    return nullptr;
}

// Evaluates all the list elements and returns their return values as a new list.
static Obj* eval_list(Kernel* kernel, Obj* env, Obj* list) {
    Obj* head = nullptr;
    Obj* tail = nullptr;
    for (Obj* lp = list; lp != Nil; lp = lp->cdr) {
        Obj* tmp = eval(kernel, env, lp->car);
        if (!tmp) {
            log("Bug: eval_list element is null");
        }
        if (head == nullptr) {
            head = tail = cons(tmp, Nil);
        } else {
            tail->cdr = cons(tmp, Nil);
            tail = tail->cdr;
        }
    }
    if (head == nullptr)
        return Nil;
    return head;
}

static bool is_list(Obj* obj) {
  return obj == Nil || obj->type == TCELL;
}

// Apply fn with args.
static Obj* apply(Kernel* kernel, Obj* env, Obj* obj, Obj* fn, Obj* args) {
    if (!is_list(args))
        error("argument must be a list");
    if (fn->type == TPRIMITIVE) {
        return fn->fn(kernel, env, args);
    }
    if (fn->type == TFUNCTION) {
        sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Apply>(env, obj, cons(fn,args)));
        return nullptr;
    }
    error("not supported");
}

// Searches for a variable by symbol. Returns null if not found.
static Obj* find(Obj* env, Obj* sym) {
    for (Obj* p = env; p; p = p->up) {
        for (Obj* cell = p->vars; cell != Nil; cell = cell->cdr) {
            Obj* bind = cell->car;
            if (sym == bind->car)
                return bind;
        }
    }
    return nullptr;
}

// Expands the given macro application form.
static Obj* macroexpand(Kernel* kernel, Obj* env, Obj* obj) {
    if (obj->type != TCELL || obj->car->type != TSYMBOL)
        return obj;
    Obj* bind = find(env, obj->car);
    if (!bind || bind->cdr->type != TMACRO)
        return obj;
    Obj* args = obj->cdr;
    Obj* body = bind->cdr->body;
    Obj* params = bind->cdr->params;
    Obj* newenv = push_env(env, params, args);
    error("Bug: macroexpand not implemented");
    return progn(kernel, newenv, body);
}

// Evaluates the S expression.
static Obj* eval(Kernel* kernel, Obj* env, Obj* obj) {
    log("eval _< _", kernel_stack(kernel), obj);
    Obj* result{};
    switch (obj->type) {
        case TINT:
        case TPRIMITIVE:
        case TFUNCTION:
        case TSPECIAL:
            // Self-evaluating objects
            result = obj;
            break;
        case TSYMBOL: {
                          // Variable
                          Obj* bind = find(env, obj);
                          if (!bind)
                              error("Undefined symbol: %s", obj->name);
                          result = bind->cdr;
                          break;
                      }
        case TCELL: {
                        // Function application form
                        Obj* expanded = macroexpand(kernel, env, obj);
                        if (expanded != obj) {
                            result = eval(kernel, env, expanded);
                            break;
                        }
                        Obj* fn = eval(kernel, env, obj->car);
                        Obj* args = obj->cdr;
                        log("function _ args _ obj _", fn, args, obj);
                        log("fn type _", fn->type);
                        if (fn->type != TPRIMITIVE && fn->type != TFUNCTION)
                            error("The head of a list must be a function");
                        result = apply(kernel, env, obj, fn, args);
                        break;
                    }
        default:
                    error("Bug: eval: Unknown tag type: %d", obj->type);
    }
    log("eval _> _", kernel_stack(kernel), result);
    return result;
}

//======================================================================
// Functions and special forms
//======================================================================

// 'expr
static Obj* prim_quote(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) != 1)
        error("Malformed quote");
    return list->car;
}

// (list expr ...)
static Obj* prim_list(Kernel* kernel, Obj* env, Obj* list) {
    return eval_list(kernel, env, list);
}

// (setq <symbol> expr)
static Obj* prim_setq(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) != 2 || list->car->type != TSYMBOL)
        error("Malformed setq");
    Obj* bind = find(env, list->car);
    if (!bind)
        error("Unbound variable %s", list->car->name);
    Obj* value = eval(kernel, env, list->cdr->car);
    bind->cdr = value;
    return value;
}

// (+ <integer> ...)
static Obj* prim_plus(Kernel* kernel, Obj* env, Obj* list) {
    int sum = 0;
    for (Obj* args = eval_list(kernel, env, list); args != Nil; args = args->cdr) {
        if (args->car->type != TINT)
            error("+ takes only numbers");
        sum += args->car->value;
    }
    return make_int(sum);
}

static Obj* handle_function(Kernel* kernel, Obj* env, Obj* list, Type type) {
    if (list->type != TCELL || !is_list(list->car) || list->cdr->type != TCELL)
        error("Malformed lambda");
    for (Obj* p = list->car; p != Nil; p = p->cdr) {
        if (p->car->type != TSYMBOL)
            error("Parameter must be a symbol");
        if (!is_list(p->cdr))
            error("Parameter list is not a flat list");
    }
    Obj* car = list->car;
    Obj* cdr = list->cdr;
    return make_function(type, car, cdr, env);
}

// (lambda (<symbol> ...) expr ...)
static Obj* prim_lambda(Kernel* kernel, Obj* env, Obj* list) {
    return handle_function(kernel, env, list, TFUNCTION);
}

static Obj* handle_defun(Kernel* kernel, Obj* env, Obj* list, Type type) {
    if (list->car->type != TSYMBOL || list->cdr->type != TCELL)
        error("Malformed defun");
    Obj* sym = list->car;
    Obj* rest = list->cdr;
    Obj* fn = handle_function(kernel, env, rest, type);
    add_variable(env, sym, fn);
    return fn;
}

// (defun <symbol> (<symbol> ...) expr ...)
static Obj* prim_defun(Kernel* kernel, Obj* env, Obj* list) {
    return handle_defun(kernel, env, list, TFUNCTION);
}

// (define <symbol> expr)
static Obj* prim_define(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) != 2 || list->car->type != TSYMBOL)
        error("Malformed define");
    Obj* sym = list->car;
    Obj* value = eval(kernel, env, list->cdr->car);
    add_variable(env, sym, value);
    return value;
}

// (defmacro <symbol> (<symbol> ...) expr ...)
static Obj* prim_defmacro(Kernel* kernel, Obj* env, Obj* list) {
    return handle_defun(kernel, env, list, TMACRO);
}

// (macroexpand expr)
static Obj* prim_macroexpand(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) != 1)
        error("Malformed macroexpand");
    Obj* body = list->car;
    return macroexpand(kernel, env, body);
}

// (println expr)
static Obj* prim_println(Kernel* kernel, Obj* env, Obj* list) {
    std::cout << eval(kernel, env, list->car) << '\n';
    return Nil;
}

// (if expr expr expr ...)
static Obj* prim_if(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) < 2)
        error("Malformed if");
    Obj* cond = eval(kernel, env, list->car);
    if (cond != Nil) {
        Obj* then = list->cdr->car;
        return eval(kernel, env, then);
    }
    Obj* els = list->cdr->cdr;
    return els == Nil ? Nil : progn(kernel, env, els);
}

// (= <integer> <integer>)
static Obj* prim_num_eq(Kernel* kernel, Obj* env, Obj* list) {
    if (list_length(list) != 2)
        error("Malformed =");
    Obj* values = eval_list(kernel, env, list);
    Obj* x = values->car;
    Obj* y = values->cdr->car;
    if (x->type != TINT || y->type != TINT)
        error("= only takes numbers");
    return x->value == y->value ? True : Nil;
}

// (exit)
static Obj* prim_exit(Kernel* kernel, Obj* env, Obj* list) {
    exit(0);
}

static Obj* prim_cons(Kernel* kernel, Obj* env, Obj* list) {
    sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Cons>(env, list));
    return nullptr;
}

static Obj* prim_car(Kernel* kernel, Obj* env, Obj* list) {
    auto result = eval(kernel, env, list->car);
    if (!result) {
        error("Bug: car async not implemented");
    }
    return result->car;
}

static Obj* prim_cdr(Kernel* kernel, Obj* env, Obj* list) {
    auto result = eval(kernel, env, list->car);
    if (!result) {
        error("Bug: cdr async not implemented");
    }
    return result->cdr;
}

static Obj* prim_is_null(Kernel* kernel, Obj* env, Obj* list) {
    return list == Nil ? True : Nil;
}

static Obj* prim_sleep(Kernel* kernel, Obj* env, Obj* list) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return Nil;
}

static void add_primitive(Obj* env, const char* name, Primitive *fn) {
    Obj* sym = intern(name);
    Obj* prim = make_primitive(fn);
    add_variable(env, sym, prim);
}

static void define_constants(Obj* env) {
    Obj* sym = intern("#t");
    add_variable(env, sym, True);
}

static void define_primitives(Obj* env) {
    add_primitive(env, "quote", prim_quote);
    add_primitive(env, "list", prim_list);
    add_primitive(env, "setq", prim_setq);
    add_primitive(env, "+", prim_plus);
    add_primitive(env, "define", prim_define);
    add_primitive(env, "defun", prim_defun);
    add_primitive(env, "defmacro", prim_defmacro);
    add_primitive(env, "macroexpand", prim_macroexpand);
    add_primitive(env, "lambda", prim_lambda);
    add_primitive(env, "if", prim_if);
    add_primitive(env, "=", prim_num_eq);
    add_primitive(env, "println", prim_println);
    add_primitive(env, "exit", prim_exit);
    add_primitive(env, "cons", prim_cons);
    add_primitive(env, "car", prim_car);
    add_primitive(env, "cdr", prim_cdr);
    add_primitive(env, "null?", prim_is_null);
    add_primitive(env, "sleep", prim_sleep);
}

class Main: public Kernel {

public:

    Main() = default;

    inline Main(int argc, char** argv, Obj* env): Kernel(env, nullptr) {
        sbn::application::string_array args;
        args.reserve(argc);
        for (int i=0; i<argc; ++i) { args.emplace_back(argv[i]); }
        if (!target_application()) {
            std::unique_ptr<sbn::application> app{new sbn::application(args, {})};
            target_application(app.release());
        }
    }

    void act() override {
        read_eval();
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result());
        print();
        read_eval();
    }

private:

    void read_eval() {
        do {
            auto expr = ::read();
            if (!expr) { sbn::commit(std::move(this_ptr())); return; }
            std::cout << "< " << expr << '\n';
            if (expr == Cparen) { error("Stray close parenthesis"); }
            if (expr == Dot) { error("Stray dot"); }
            expression(expr);
            result(eval(this, environment(), expr));
            if (result()) {
                print();
                //sbn::commit(std::move(this_ptr()));
            }
        } while (result());
    }

    void print() {
        std::cout << "> " << result() << '\n';
    }

};

Obj* make_initial_environment() {
    // Constants and primitives
    Nil = make_special(TNIL);
    Dot = make_special(TDOT);
    Cparen = make_special(TCPAREN);
    True = make_special(TTRUE);
    Symbols = Nil;
    Obj* env = make_env(Nil, nullptr);
    define_constants(env);
    define_primitives(env);
    return env;
}

int main(int argc, char** argv) {
    Obj* env = make_initial_environment();
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        factory.local().num_upstream_threads(4);
        factory.local().num_downstream_threads(0);
    }
    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv, env));
    }
    return wait_and_return();
}
