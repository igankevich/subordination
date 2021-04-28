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
#include <unordered_map>

#include <unistdx/system/linker>

#include <subordination/api.hh>
#include <subordination/core/error_handler.hh>
#include <subordination/core/kernel.hh>
#include <subordination/minilisp/minilisp.hh>

namespace {

    template <class ... Args>
    inline void
    log(const Args& ... args) {
        sys::log_message("lisp", args...);
    }

    lisp::Special* Nil = nullptr;
    lisp::Special* Dot = nullptr;
    lisp::Special* Cparen = nullptr;
    lisp::Special* True = nullptr;
    lisp::Object* Symbols = nullptr;

    std::mutex global_mutex;
    std::unordered_map<lisp::Cpp_function_id,lisp::Cpp_function> functions;
}

//======================================================================
// Lisp objects
//======================================================================

using lisp::Environment;
using lisp::Function;
using lisp::Integer;
using lisp::Kernel;
using lisp::List;
using lisp::Object;
using lisp::Primitive;
using lisp::Special;
using lisp::Special_type;
using lisp::Symbol;
using lisp::Type;
using lisp::cons;

const char* lisp::to_string(Type t) noexcept {
    switch (t) {
        case Type::Integer: return "integer";
        case Type::Cell: return "cell";
        case Type::Symbol: return "symbol";
        case Type::Primitive: return "primitive";
        case Type::Function: return "function";
        case Type::Macro: return "macro";
        case Type::Special: return "special";
        case Type::Environment: return "environment";
        default: return nullptr;
    }
}

std::ostream& lisp::operator<<(std::ostream& out, Type rhs) {
    auto s = to_string(rhs);
    return out << (s ? s : "unknown");
}

const char* lisp::to_string(Special_type t) noexcept {
    switch (t) {
        case Special_type::Nil: return "nil";
        case Special_type::Dot: return "dot";
        case Special_type::Cparen: return "cparen";
        case Special_type::True: return "true";
        default: return nullptr;
    }
}

std::ostream& lisp::operator<<(std::ostream& out, Special_type rhs) {
    auto s = to_string(rhs);
    return out << (s ? s : "unknown");
}

lisp::Special* lisp::make(Special_type type) noexcept {
    switch (type) {
        case Special_type::Nil: return Nil;
        case Special_type::Dot: return Dot;
        case Special_type::Cparen: return Cparen;
        case Special_type::True: return True;
        default: return nullptr;
    }
}

static Object* eval_list(Kernel* kernel, Environment* env, Object* list);
static Object* progn(Kernel* kernel, Environment* env, Object* list);
static size_t list_length(const Object* list);

Object* vector_to_list(const std::vector<Object*>& vec) {
    const auto n = vec.size();
    if (n == 0) { return Nil; }
    Object* list = Nil;
    for (size_t i=0; i<n; ++i) {
        size_t j = n-1-i;
        list = lisp::cons(vec[j], list);
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
    Environment* _environment{};
    Object* _expression{};
    Object* _result = Nil;
    size_t _index = 0;

public:
    Kernel() = default;
    inline Kernel(Environment* env, Object* expr): Kernel(env, expr, 0) {}
    inline Kernel(Environment* env, Object* expr, size_t index):
    _environment(env), _expression(expr), _index(index) {}

    void act() override {
        this->_result = this->_expression->eval(this, this->_environment);
        if (this->_result) { sbn::commit(std::move(this_ptr())); }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result());
        sbn::commit(std::move(this_ptr()));
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        this->_environment->write(out);
        this->_expression->write(out);
        this->_result->write(out);
        out.write(this->_index);
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        this->_environment = cast<Environment>(::lisp::read(in));
        this->_expression = ::lisp::read(in);
        this->_result = ::lisp::read(in);
        in.read(this->_index);
    }

    inline Object* result() noexcept { return this->_result; }
    inline const Object* result() const noexcept { return this->_result; }
    inline void result(Object* rhs) noexcept { this->_result = rhs; }
    inline Environment* environment() noexcept { return this->_environment; }
    inline const Environment* environment() const noexcept { return this->_environment; }
    inline Object* expression() noexcept { return this->_expression; }
    inline const Object* expression() const noexcept { return this->_expression; }
    inline void expression(Object* rhs) noexcept { this->_expression = rhs; }
    inline size_t index() const noexcept { return this->_index; }
    inline void index(size_t rhs) noexcept { this->_index = rhs; }

};

class Cons: public Kernel {

private:
    Object* _a{};
    Object* _b{};
    Object* _result_a{};
    Object* _result_b{};
    std::atomic<int> _count{0};

public:
    Cons() = default;
    inline Cons(Environment* env, Object* expr): Kernel(env, expr),
    _a(expr->car), _b(expr->cdr->car) {}

    void act() override {
        this->_result_a = this->_a->eval(this, environment());
        this->_result_b = this->_b->eval(this, environment());
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
    inline Progn(Environment* env, Object* expr): Kernel(env, expr) {}

    void act() override {
        auto list = expression();
        while (list != Nil) {
            auto element = list->car;
            list = list->cdr;
            expression(list);
            result(element->eval(this, environment()));
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
    Object* _new_expr{};
    std::vector<Object*> _args;
    enum class State {Args,Progn} _state = State::Args;
public:
    Apply() = default;

    inline Apply(Environment* env, Object* expr, Object* new_expr):
    Kernel(env, expr), _new_expr(new_expr) {}

    void act() override {
        auto args = this->_new_expr->cdr;
        auto n = list_length(args);
        this->_args.resize(n);
        size_t i = 0;
        bool async = false;
        for (Object* lp = args; lp != Nil; lp = lp->cdr, ++i) {
            auto result = lp->car->eval(this, environment());
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
        auto tmp = reinterpret_cast<Function*>(fn);
        Object* body = tmp->body;
        auto* params = tmp->params;
        Object* eargs = vector_to_list(this->_args);
        Environment* newenv = new Environment(params, eargs, tmp->env);
        //log("apply _ to _", fn, eargs);
        result(progn(this, newenv, body));
        if (result()) {
            log("_ < xapply _ > _", kernel_stack(this), expression(), result());
            sbn::commit(std::move(this_ptr()));
        }
    }

};

[[noreturn]] static void error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

//======================================================================
// Constructors
//======================================================================

static Function* make_function(Type type, Symbol* params, Object* body, Environment* env) {
    assert(type == Type::Function || type == Type::Macro);
    auto* r = new Function(type);
    r->params = params;
    r->body = body;
    r->env = env;
    return r;
}

// Returns ((x . y) . a)
static Object* acons(Object* x, Object* y, Object* a) {
    return cons(cons(x, y), a);
}

lisp::Environment::Environment(Symbol* symbols, Object* values, Environment* parent) {
    this->type = Type::Environment;
    if (list_length(symbols) != list_length(values))
        error("Cannot apply function: number of argument does not match");
    Object* map = Nil;
    Object* p = symbols;
    Object* q = values;
    for (; p != Nil; p = reinterpret_cast<Symbol*>(p->cdr), q = q->cdr) {
        Object* sym = p->car;
        Object* val = q->car;
        map = acons(sym, val, map);
    }
    this->vars = map;
    this->parent = parent;
}

lisp::Cell* lisp::cons(Object* car, Object* cdr) {
    auto* cell = new Cell;
    cell->type = Type::Cell;
    cell->car = car;
    cell->cdr = cdr;
    return cell;
}

//======================================================================
// Parser
//
// This is a hand-written recursive-descendent parser.
//======================================================================

static Object* read();

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
static Object* read_list() {
    Object* obj = read();
    if (!obj)
        error("Unclosed parenthesis");
    if (obj == Dot)
        error("Stray dot");
    if (obj == Cparen)
        return Nil;
    Object* head, *tail;
    head = tail = cons(obj, Nil);

    for (;;) {
        Object* obj = read();
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
static Symbol* intern(const char* name) {
    std::lock_guard<std::mutex> lock(::global_mutex);
    for (Object* p = ::Symbols; p != Nil; p = p->cdr) {
        auto tmp = reinterpret_cast<Symbol*>(p->car);
        if (name == tmp->name) {
            return tmp;
        }
    }
    auto* sym = new Symbol(name);
    ::Symbols = cons(sym, ::Symbols);
    return sym;
}

// Reader marcro ' (single quote). It reads an expression and returns (quote <expr>).
static Object* read_quote() {
    auto* sym = intern("quote");
    return cons(sym, cons(read(), Nil));
}

static int read_number(int val) {
    while (isdigit(peek()))
        val = val * 10 + (getchar() - '0');
    return val;
}

#define SYMBOL_MAX_LEN 200

static Symbol* read_symbol(char c) {
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

static Object* read() {
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
            return new Integer(read_number(c - '0'));
        if (c == '-')
            return new Integer(-read_number(0));
        if (isalpha(c) || strchr("+=!@#$%^&*", c))
            return read_symbol(c);
        error("Don't know how to handle %c", c);
    }
}

void lisp::Environment::write(std::ostream& out) const {
    for (auto p = this; p; p = p->parent) {
        p->vars->write(out);
    }
}

void lisp::Special::write(std::ostream& out) const {
    if (this == Nil) {
        out << "()";
    } else if (this == True) {
        out << "#t";
    } else {
        error("Bug: print: Unknown subtype: %d", this->subtype);
    }
}

void lisp::Integer::write(std::ostream& out) const {
    out << value;
}

void lisp::Cell::write(std::ostream& out) const {
    out << '(';
    for (const Object* obj = this;;) {
        out << obj->car;
        if (obj->cdr == Nil)
            break;
        if (!obj->cdr) {
            out << " . nullptr";
            break;
        }
        if (obj->cdr->type != Type::Cell) {
            out << " . " << obj->cdr;
            break;
        }
        out << ' ';
        obj = obj->cdr;
    }
    out << ')';
}

void lisp::Symbol::write(std::ostream& out) const {
    out << name;
}

void lisp::Primitive::write(std::ostream& out) const {
    out << "<primitive> " << dl::symbol(reinterpret_cast<void*>(fn)).name();
}

void lisp::Function::write(std::ostream& out) const {
    switch (type) {
        case Type::Function:
            out << "<function>";
            break;
        case Type::Macro:
            out << "<macro>";
            break;
        default:
            error("Bug: print: Unknown tag type: %d", type);
    }
}

std::ostream& lisp::operator<<(std::ostream& out, const Object* obj) {
    if (obj) {
        obj->write(out);
    } else {
        out << "nullptr";
    }
    return out;
}

std::string object_to_string(const Object* obj) {
    std::stringstream tmp;
    tmp << obj;
    return tmp.str();
}

static size_t list_length(const Object* list) {
    size_t len = 0;
    for (;;) {
        if (list == Nil)
            return len;
        if (list->type != Type::Cell)
            error("length: cannot handle dotted list");
        list = list->cdr;
        ++len;
    }
    return len;
}

size_t lisp::List::size() const {
    return list_length(this);
}

//======================================================================
// Evaluator
//======================================================================

void lisp::Environment::add(Symbol* symbol, Object* value) {
    this->vars = acons(symbol, value, this->vars);
}

// Evaluates the list elements from head and returns the last return value.
static Object* progn(Kernel* kernel, Environment* env, Object* list) {
    if (list == Nil) { return Nil; }
    if (list->cdr == Nil) { return list->car->eval(kernel, env); }
    sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Progn>(env, list));
    return nullptr;
}

// Evaluates all the list elements and returns their return values as a new list.
static Object* eval_list(Kernel* kernel, Environment* env, Object* list) {
    Object* head = nullptr;
    Object* tail = nullptr;
    for (Object* lp = list; lp != Nil; lp = lp->cdr) {
        Object* tmp = lp->car->eval(kernel, env);
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

static bool is_list(Object* obj) {
    return obj == Nil || obj->type == Type::Cell;
}

// Apply fn with args.
static Object* apply(Kernel* kernel, Environment* env, Object* obj, Object* fn, Object* args) {
    if (!is_list(args))
        error("argument must be a list");
    if (fn->type == Type::Primitive) {
        auto tmp = reinterpret_cast<Primitive*>(fn);
        return tmp->fn(kernel, env, args);
    }
    if (fn->type == Type::Function) {
        sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Apply>(env, obj, cons(fn,args)));
        return nullptr;
    }
    error("not supported");
}

Object* lisp::Environment::find(Symbol* symbol) {
    for (Environment* p = this; p; p = p->parent) {
        for (Object* cell = p->vars; cell != Nil; cell = cell->cdr) {
            Object* bind = cell->car;
            if (symbol == bind->car) {
                return bind;
            }
        }
    }
    return nullptr;
}

// Expands the given macro application form.
static Object* macroexpand(Kernel* kernel, Environment* env, Object* obj) {
    if (obj->type != Type::Cell || obj->car->type != Type::Symbol)
        return obj;
    Object* bind = env->find(reinterpret_cast<Symbol*>(obj->car));
    if (!bind || bind->cdr->type != Type::Macro)
        return obj;
    Object* args = obj->cdr;
    auto tmp = reinterpret_cast<Function*>(bind->cdr);
    Object* body = tmp->body;
    auto* params = tmp->params;
    auto* newenv = new Environment(params, args, env);
    error("Bug: macroexpand not implemented");
    return progn(kernel, newenv, body);
}

Object* lisp::make(Type type) {
    switch (type) {
        case Type::Integer: return new Integer;
        case Type::Cell: return new Cell;
        case Type::Symbol: return new Symbol;
        case Type::Primitive: return new Primitive;
        case Type::Function: return new Function(type);
        case Type::Macro: return new Function(type);
        case Type::Special: return nullptr;
        case Type::Environment: return new Environment(Nil, nullptr);
        default: return nullptr;
    }
}

void lisp::Object::write(sbn::kernel_buffer& out) const {
    out.write(this->type);
}

Object* lisp::read(sbn::kernel_buffer& in) {
    Type type{};
    in.read(type);
    Special_type subtype{};
    if (type == Type::Special) {
        in.read(subtype);
        return make(subtype);
    } else {
        return make(type);
    }
}

void lisp::Object::read(sbn::kernel_buffer& in) {
}

void lisp::Special::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->subtype);
}

void lisp::Special::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in.read(this->subtype);
}

void lisp::Cell::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    this->car->write(out);
    this->cdr->write(out);
}

void lisp::Cell::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    this->car = ::lisp::read(in);
    this->cdr = ::lisp::read(in);
}

void lisp::Primitive::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->_id);
}

void lisp::Primitive::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in.read(this->_id);
    auto result = functions.find(this->_id);
    if (result == functions.end()) {
        throw std::runtime_error("bad function id");
    }
    this->fn = result->second;
}

void lisp::Function::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    this->params->write(out);
    this->body->write(out);
    this->env->write(out);
}

void lisp::Function::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    this->params = cast<Symbol>(::lisp::read(in));
    this->body = ::lisp::read(in);
    this->env = cast<Environment>(::lisp::read(in));
}

void lisp::Integer::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->value);
}

void lisp::Integer::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in.read(this->value);
}

void lisp::Symbol::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->name);
}

void lisp::Symbol::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in.read(this->name);
}

void lisp::Environment::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    this->vars->write(out);
    this->parent->write(out);
}

void lisp::Environment::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    this->vars = ::lisp::read(in);
    this->parent = cast<Environment>(::lisp::read(in));
}

Object* lisp::Object::eval(Kernel* kernel, Environment* env) {
    log("eval _< _", kernel_stack(kernel), this);
    auto result = this;
    log("eval _> _", kernel_stack(kernel), result);
    return result;
}

Object* lisp::Symbol::eval(Kernel* kernel, Environment* env) {
    log("eval _< _", kernel_stack(kernel), this);
    Object* bind = env->find(this);
    if (!bind) {
        error("Undefined symbol: %s", this->name.data());
    }
    auto result = bind->cdr;
    log("eval _> _", kernel_stack(kernel), result);
    return result;
}

Object* lisp::Cell::eval(Kernel* kernel, Environment* env) {
    log("eval _< _", kernel_stack(kernel), this);
    // Function application form
    Object* expanded = macroexpand(kernel, env, this);
    Object* result{};
    if (expanded != this) {
        result = expanded->eval(kernel, env);
    } else {
        Object* fn = this->car->eval(kernel, env);
        Object* args = this->cdr;
        log("function _ args _ obj _", fn, args, this);
        log("fn type _", fn->type);
        if (fn->type != Type::Primitive && fn->type != Type::Function)
            error("The head of a list must be a function");
        result = apply(kernel, env, this, fn, args);
    }
    log("eval _> _", kernel_stack(kernel), result);
    return result;
}

//======================================================================
// Functions and special forms
//======================================================================

// 'expr
static Object* prim_quote(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 1)
        error("Malformed quote");
    return list->car;
}

// (list expr ...)
static Object* prim_list(Kernel* kernel, Environment* env, Object* list) {
    return eval_list(kernel, env, list);
}

// (setq <symbol> expr)
static Object* prim_setq(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 2 || list->car->type != Type::Symbol) {
        error("Malformed setq");
    }
    auto tmp = reinterpret_cast<Symbol*>(list->car);
    Object* bind = env->find(tmp);
    if (!bind) {
        error("Unbound variable %s", tmp->name.data());
    }
    Object* value = list->cdr->car->eval(kernel, env);
    bind->cdr = value;
    return value;
}

// (+ <integer> ...)
static Object* prim_plus(Kernel* kernel, Environment* env, Object* list) {
    int sum = 0;
    for (Object* args = eval_list(kernel, env, list); args != Nil; args = args->cdr) {
        if (args->car->type != Type::Integer) {
            error("+ takes only numbers");
        }
        auto tmp = reinterpret_cast<Integer*>(args->car);
        sum += tmp->value;
    }
    return new Integer(sum);
}

static Object* handle_function(Kernel* kernel, Environment* env, Object* list, Type type) {
    if (list->type != Type::Cell || !is_list(list->car) || list->cdr->type != Type::Cell)
        error("Malformed lambda");
    for (Object* p = list->car; p != Nil; p = p->cdr) {
        if (p->car->type != Type::Symbol) {
            error("Parameter must be a symbol");
        }
        if (!is_list(p->cdr)) {
            error("Parameter list is not a flat list");
        }
    }
    auto* symbols = reinterpret_cast<Symbol*>(list->car);
    Object* cdr = list->cdr;
    return make_function(type, symbols, cdr, env);
}

// (lambda (<symbol> ...) expr ...)
static Object* prim_lambda(Kernel* kernel, Environment* env, Object* list) {
    return handle_function(kernel, env, list, Type::Function);
}

static Object* handle_defun(Kernel* kernel, Environment* env, Object* list, Type type) {
    if (list->car->type != Type::Symbol || list->cdr->type != Type::Cell)
        error("Malformed defun");
    auto* sym = reinterpret_cast<Symbol*>(list->car);
    Object* rest = list->cdr;
    Object* fn = handle_function(kernel, env, rest, type);
    env->add(sym, fn);
    return fn;
}

// (defun <symbol> (<symbol> ...) expr ...)
static Object* prim_defun(Kernel* kernel, Environment* env, Object* list) {
    return handle_defun(kernel, env, list, Type::Function);
}

// (define <symbol> expr)
static Object* prim_define(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 2 || list->car->type != Type::Symbol)
        error("Malformed define");
    auto* sym = reinterpret_cast<Symbol*>(list->car);
    Object* value = list->cdr->car->eval(kernel, env);
    env->add(sym, value);
    return value;
}

// (defmacro <symbol> (<symbol> ...) expr ...)
static Object* prim_defmacro(Kernel* kernel, Environment* env, Object* list) {
    return handle_defun(kernel, env, list, Type::Macro);
}

// (macroexpand expr)
static Object* prim_macroexpand(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 1)
        error("Malformed macroexpand");
    Object* body = list->car;
    return macroexpand(kernel, env, body);
}

// (println expr)
static Object* prim_println(Kernel* kernel, Environment* env, Object* list) {
    std::cout << list->car->eval(kernel, env) << '\n';
    return Nil;
}

// (if expr expr expr ...)
static Object* prim_if(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) < 2)
        error("Malformed if");
    Object* cond = list->car->eval(kernel, env);
    if (cond != Nil) {
        Object* then = list->cdr->car;
        return then->eval(kernel, env);
    }
    Object* els = list->cdr->cdr;
    return els == Nil ? Nil : progn(kernel, env, els);
}

// (= <integer> <integer>)
static Object* prim_num_eq(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 2)
        error("Malformed =");
    Object* values = eval_list(kernel, env, list);
    Object* x = values->car;
    Object* y = values->cdr->car;
    if (x->type != Type::Integer || y->type != Type::Integer)
        error("= only takes numbers");
    auto a = reinterpret_cast<Integer*>(x);
    auto b = reinterpret_cast<Integer*>(y);
    return a->value == b->value ? True : Nil;
}

// (exit)
static Object* prim_exit(Kernel* kernel, Environment* env, Object* list) {
    exit(0);
}

static Object* prim_cons(Kernel* kernel, Environment* env, Object* list) {
    sbn::upstream<sbn::Local>(kernel, sbn::make_pointer<Cons>(env, list));
    return nullptr;
}

static Object* prim_car(Kernel* kernel, Environment* env, Object* list) {
    auto result = list->car->eval(kernel, env);
    if (!result) {
        error("Bug: car async not implemented");
    }
    return result->car;
}

static Object* prim_cdr(Kernel* kernel, Environment* env, Object* list) {
    auto result = list->car->eval(kernel, env);
    if (!result) {
        error("Bug: cdr async not implemented");
    }
    return result->cdr;
}

static Object* prim_is_null(Kernel* kernel, Environment* env, Object* list) {
    return list == Nil ? True : Nil;
}

void lisp::define(Environment* env, const char* name, Object* value) {
    env->add(intern(name), value);
}

void lisp::define(Environment* env, const char* name, Cpp_function function, Cpp_function_id id) {
    {
        std::lock_guard<std::mutex> lock(global_mutex);
        auto result = functions.emplace(id, function);
        if (!result.second) {
            throw std::invalid_argument("duplicate function id");
        }
    }
    env->add(intern(name), new Primitive(function,id));
}

static void define_constants(Environment* env) {
    env->add(intern("#t"), True);
}

static void define_primitives(Environment* env) {
    define(env, "quote", prim_quote, 0);
    define(env, "list", prim_list, 1);
    define(env, "setq", prim_setq, 2);
    define(env, "+", prim_plus, 3);
    define(env, "define", prim_define, 4);
    define(env, "defun", prim_defun, 5);
    define(env, "defmacro", prim_defmacro, 6);
    define(env, "macroexpand", prim_macroexpand, 7);
    define(env, "lambda", prim_lambda, 8);
    define(env, "if", prim_if, 9);
    define(env, "=", prim_num_eq, 10);
    define(env, "println", prim_println, 11);
    define(env, "exit", prim_exit, 12);
    define(env, "cons", prim_cons, 13);
    define(env, "car", prim_car, 14);
    define(env, "cdr", prim_cdr, 15);
    define(env, "null?", prim_is_null, 16);
    define(env, "sleep", [] (Kernel* kernel, Environment* env, List* args) -> Object* {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return Nil;
    }, 1000);
}

class Main: public Kernel {

public:

    Main() = default;

    inline Main(int argc, char** argv, Environment* env): Kernel(env, nullptr) {
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
            result(expr->eval(this, environment()));
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

Environment* make_initial_environment() {
    // Constants and primitives
    Nil = new Special(Special_type::Nil);
    Dot = new Special(Special_type::Dot);
    Cparen = new Special(Special_type::Cparen);
    True = new Special(Special_type::True);
    ::Symbols = Nil;
    Environment* env = new Environment(Nil, nullptr);
    define_constants(env);
    define_primitives(env);
    return env;
}

int main(int argc, char** argv) {
    Environment* env = make_initial_environment();
    using namespace sbn;
    install_error_handler();
    {
        auto g = factory.types().guard();
        factory.types().add<Main>(1);
        factory.local().num_upstream_threads(1);
        factory.local().num_downstream_threads(0);
    }
    factory_guard g;
    if (this_application::standalone()) {
        send(sbn::make_pointer<Main>(argc, argv, env));
    }
    return wait_and_return();
}
