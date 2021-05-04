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

#include <sys/mman.h>
#include <unistdx/system/linker>

#include <subordination/api.hh>
#include <subordination/core/kernel.hh>
#include <subordination/minilisp/minilisp.hh>

namespace {

    template <class ... Args>
    inline void
    log(const Args& ... args) {
        sys::log_message("lisp", args...);
    }

    inline void print(std::ostream&) {}

    template <class Head, class ... Tail> inline void
    print(std::ostream& out, const Head& head, const Tail& ... tail) {
        out << head;
        print(out, tail...);
    }

    template <class ... Args> [[noreturn]] void
    error(const Args& ... args) {
        std::stringstream tmp;
        print(tmp, args...);
        //sbn::exit(1);
        throw std::runtime_error(tmp.str());
    }

    lisp::Symbol* Dot = nullptr;
    lisp::Boolean* True = nullptr;
    lisp::Boolean* False = nullptr;
    lisp::Object* Symbols = nullptr;
    lisp::Environment* Top = nullptr;
    lisp::Object* Async = nullptr;

    std::mutex global_mutex;
    std::unordered_map<lisp::Cpp_function_id,lisp::Cpp_function> functions;
    std::unordered_map<std::string,lisp::Symbol*> symbols;

    std::string to_string(std::istream& in) {
        std::stringstream tmp;
        tmp << in.rdbuf();
        return tmp.str();
    }

}

//======================================================================
// Lisp objects
//======================================================================

using lisp::Cell;
using lisp::Environment;
using lisp::Function;
using lisp::Integer;
using lisp::Kernel;
using lisp::List;
using lisp::Object;
using lisp::Primitive;
using lisp::Symbol;
using lisp::Type;
using lisp::cons;
using lisp::read;
using lisp::Result;

const char* lisp::to_string(Type t) noexcept {
    switch (t) {
        case Type::Void: return "void";
        case Type::Integer: return "integer";
        case Type::Cell: return "cell";
        case Type::Symbol: return "symbol";
        case Type::Primitive: return "primitive";
        case Type::Function: return "function";
        case Type::Macro: return "macro";
        case Type::Environment: return "environment";
        case Type::Boolean: return "boolean";
        case Type::Async: return "async";
        default: return nullptr;
    }
}

std::ostream& lisp::operator<<(std::ostream& out, Type rhs) {
    auto s = to_string(rhs);
    return out << (s ? s : "unknown");
}

std::ostream& lisp::operator<<(std::ostream& out, const Result& result) {
    out << '[';
    out << result.object;
    out << ',';
    out << result.kernel;
    out << ']';
    return out;
}

static Object* eval_list(Kernel* kernel, Environment* env, Object* list);
auto progn(Kernel* kernel, Environment* env, Object* list) -> Result;
static size_t list_length(const Object* list);

Object* vector_to_list(const std::vector<Object*>& vec) {
    const auto n = vec.size();
    if (n == 0) { return nullptr; }
    Object* list = nullptr;
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
        std::string tmp;
        tmp += typeid(*k).name() + std::string(" ");
        s = tmp + s;
    }
    return s;
}

void lisp::Kernel::act() {
    auto res = this->_expression->eval(this, this->_environment);
    if (!res.asynchronous()) {
        this->_result = res.object;
        sbn::commit(std::move(this_ptr()));
    }
}

void lisp::Kernel::react(sbn::kernel_ptr&& k) {
    auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
    result(child->result());
    sbn::commit(std::move(this_ptr()));
}

void lisp::Kernel::write(sbn::kernel_buffer& out) const {
    sbn::kernel::write(out);
    this->_environment->write(out);
    this->_expression->write(out);
    this->_result->write(out);
    out.write(this->_index);
}

void lisp::Kernel::read(sbn::kernel_buffer& in) {
    sbn::kernel::read(in);
    this->_environment = cast<Environment>(::lisp::read(in));
    this->_expression = ::lisp::read(in);
    this->_result = ::lisp::read(in);
    in.read(this->_index);
}

void lisp::Main::act() {
    this->_current_script = ::to_string(std::cin);
    this->_view = string_view(this->_current_script);
    if (auto a = target_application()) {
        for (auto arg : a->arguments()) {
            std::clog << "arg=" << arg << std::endl;
        }
    }
    read_eval();
}

void lisp::Main::react(sbn::kernel_ptr&& k) {
    auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
    result(child->result());
    print();
    read_eval();
}

void lisp::Main::read_eval() {
    do {
        this->_view.trim();
        if (this->_view.empty()) { break; }
        auto expr = ::lisp::read(this->_view);
        std::cout << "< " << expr << '\n';
        expression(expr);
        if (!expr) {
            result(expr);
        } else {
            auto res = expr->eval(this, environment());
            if (res.asynchronous()) { return; }
            result(res.object);
        }
        print();
    } while (result());
    sbn::commit(std::move(this_ptr()));
}

void lisp::Main::print() {
    std::cout << "> " << result() << '\n';
}


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
        auto res_a = this->_a->eval(this, environment());
        if (res_a.asynchronous()) {
            this->_result_a = res_a.kernel->expression();
        } else {
            this->_result_a = res_a.object;
            ++this->_count;
        }
        auto res_b = this->_b->eval(this, environment());
        if (res_b.asynchronous()) {
            this->_result_b = res_b.kernel->expression();
        } else {
            this->_result_b = res_b.object;
            ++this->_count;
        }
        if (this->_count == 2) {
            result(cons(this->_result_a, this->_result_b));
            sbn::commit(std::move(this_ptr()));
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        bool success = false;
        if (child->expression() == this->_result_a) {
            this->_result_a = child->result();
            success = true;
        }
        if (child->expression() == this->_result_b) {
            this->_result_b = child->result();
            success = true;
        }
        if (!success) {
            log("error child _", typeid(*child).name());
            Assert(success);
        }
        ++this->_count;
        if (this->_count == 2) {
            result(cons(this->_result_a, this->_result_b));
            sbn::commit(std::move(this_ptr()));
        }
    }

};

class Car: public Kernel {

public:
    Car() = default;
    inline Car(Environment* env, Object* expr): Kernel(env, expr) {}

    void act() override {
        auto result = expression()->car->eval(this, environment());
        if (!result.asynchronous()) {
            this->result(result.object->car);
            sbn::commit(std::move(this_ptr()));
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result()->car);
        sbn::commit(std::move(this_ptr()));
    }

};

class Progn: public Kernel {
public:
    Progn() = default;
    inline Progn(Environment* env, Object* expr): Kernel(env, expr) {}

    void act() override {
        auto list = expression();
        while (list != nullptr) {
            auto element = list->car;
            list = list->cdr;
            expression(list);
            auto res = element->eval(this, environment());
            if (res.asynchronous()) { return; }
            result(res.object);
        }
        sbn::commit(std::move(this_ptr()));
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        result(child->result());
        act();
    }

};

class Apply: public Kernel {
private:
    Object* _new_expr{};
    std::vector<Object*> _args;
    sys::u64 _count = 0;
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
        for (Object* lp = args; lp != nullptr; lp = lp->cdr, ++i) {
            auto res = lp->car->eval(this, environment());
            if (res.asynchronous()) {
                this->_args[i] = res.kernel->expression();
            } else {
                this->_args[i] = res.object;
                ++this->_count;
            }
        }
        if (this->_count == this->_args.size()) {
            postamble();
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        auto child = sbn::pointer_dynamic_cast<Kernel>(std::move(k));
        if (this->_state == State::Args) {
            const auto n = this->_args.size();
            bool success = false;
            for (size_t i=0; i<n; ++i) {
                if (this->_args[i] == child->expression()) {
                    this->_args[i] = child->result();
                    success = true;
                    ++this->_count;
                }
            }
            if (!success) {
                log("error child _", typeid(*child).name());
                Assert(success);
            }
            if (this->_count == this->_args.size()) {
                postamble();
            }
        } else {
            result(child->result());
            log("result _", result());
            sbn::commit(std::move(this_ptr()));
        }
    }

private:
    void postamble() {
        this->_state = State::Progn;
        auto fn = this->_new_expr->car;
        auto tmp = reinterpret_cast<Function*>(fn);
        Object* body = tmp->body;
        auto* params = tmp->params;
        Object* eargs = vector_to_list(this->_args);
        Environment* newenv = new Environment(params, eargs, tmp->env);
        auto res = progn(this, newenv, body);
        if (!res.asynchronous()) {
            result(res.object);
            sbn::commit(std::move(this_ptr()));
        }
    }

};

//======================================================================
// Constructors
//======================================================================

static Function* make_function(Type type, Cell* params, Object* body, Environment* env) {
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

lisp::Environment::Environment(Cell* symbols, Object* values, Environment* parent) {
    this->type = Type::Environment;
    if (list_length(symbols) != list_length(values))
        error("Cannot apply function: number of argument does not match");
    Object* map = nullptr;
    Object* p = symbols;
    Object* q = values;
    for (; p != nullptr; p = p->cdr, q = q->cdr) {
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


lisp::Object* lisp::equal(Object* a, Object* b) {
    Expects(a);
    Expects(b);
    return new Boolean(a->equals(b));
}

lisp::Symbol* lisp::intern(lisp::string_view name) {
    std::string key(name.first, name.last);
    std::lock_guard<std::mutex> lock(::global_mutex);
    auto result = symbols.find(key);
    if (result == symbols.end()) {
        std::tie(result,std::ignore) = symbols.emplace(
            key, new Symbol(name));
    }
    return result->second;
}

Object* lisp::read(const char* string) {
    string_view s(string);
    return read(s);
}

std::ostream& lisp::operator<<(std::ostream& out, const string_view& rhs) {
    auto first = rhs.orig;
    while (*first) {
        if (first == rhs.first) {
            out << "---[";
            out.put(*first);
            if (first == rhs.last) {
                out << "]---";
            }
        } else if (first == rhs.last) {
            out.put(*first);
            out << "]---";
        } else {
            out.put(*first);
        }
        ++first;
    }
    return out;
}

bool lisp::string_view::operator==(string_view rhs) const noexcept  {
    return size() == rhs.size() && std::equal(first, last, rhs.first);
}

void lisp::string_view::trim_left() {
    while (first != last && std::isspace(*first)) { ++first; }
}

void lisp::string_view::trim() {
    trim_left();
    while (first < last && std::isspace(*(last-1))) { --last; }
}

void lisp::string_view::skip_until_the_end_of_the_line() {
    while (first != last && *first != '\n' && *first != '\r') { ++first; }
    trim_left();
}

void lisp::string_view::skip_until_the_next_white_space() {
    while (first != last && !std::isspace(*first)) { ++first; }
}

const char* lisp::string_view::scan_for_matching_bracket() const {
    int counter = 0;
    bool inside_string = false, inside_comment = false;
    auto a = first, b = last;
    char prev = 0;
    while (a != b) {
        const auto ch = *a;
        if (inside_string) {
            if (ch == '"' && prev != '\\') {
                inside_string = false;
            }
            prev = ch;
        } else if (inside_comment) {
            if (ch == '\n' || ch == '\r') {
                inside_comment = false;
            }
        } else {
            if (ch == '(') {
                ++counter;
            } else if (ch == ')') {
                --counter;
                if (counter < 0) {
                    std::stringstream tmp;
                    tmp << "Invalid lisp expression: " << *this;
                    throw std::invalid_argument(tmp.str());
                }
            } else if (ch == '"') {
                inside_string = true;
            } else if (ch == ';') {
                inside_comment = true;
            }
        }
        ++a;
        if (counter == 0) { break; }
    }
    return a;
}

Object* lisp::string_view::read_list() {
    if (first == last) { return nullptr; }
    ++first, --last;
    if (!(first < last)) {
        std::stringstream tmp;
        tmp << "Invalid lisp list: " << *this;
        throw std::invalid_argument(tmp.str());
    }
    auto head = new Cell(read(*this), nullptr);
    Object* tail = head;
    while (first != last) {
        auto obj = read(*this);
        if (obj == Dot) {
            tail->cdr = read(*this);
        } else {
            tail->cdr = cons(obj, nullptr);
            tail = tail->cdr;
        }
    }
    return head;
}

bool lisp::string_view::is_integer() const noexcept {
    if (first == last) { return false; }
    auto a = first, b = last;
    // check the first character
    auto first_character_is_digit = std::isdigit(*a);
    if (a != b && !(first_character_is_digit || *a == '-' || *a == '+')) { return false; }
    ++a;
    if (a == b && !first_character_is_digit) { return false; }
    // check the remaining characters
    while (a != b) {
        if (!(std::isdigit(*a))) { return false; }
        ++a;
    }
    return true;
}

bool lisp::string_view::is_symbol() const noexcept {
    if (first == last) { return false; }
    auto a = first, b = last;
    while (a != b) {
        if (std::isspace(*a) || *a == ';') {
            return false;
        }
        ++a;
    }
    return true;
}

Object* lisp::string_view::read_atom() {
    if (first == last) {
        std::stringstream tmp;
        tmp << "Invalid lisp list: " << *this;
        throw std::invalid_argument(tmp.str());
    }
    if (is_integer()) {
        int64_t i = 0;
        std::stringstream tmp;
        tmp.write(first, last-first);
        tmp >> i;
        return new Integer(i);
    }
    return intern(*this);
}

Object* lisp::read(string_view& s) {
    s.trim();
    while (s.first != s.last) {
        auto ch = *s.first;
        if (ch == ';') {
            ++s.first;
            s.skip_until_the_end_of_the_line();
        } else if (ch == '(') {
            auto new_last = s.scan_for_matching_bracket();
            auto old_first = s.first;
            s.first = new_last;
            return string_view(old_first, new_last, s.orig).read_list();
        } else if (ch == '.') {
            ++s.first;
            return Dot;
        } else if (ch == '\'') {
            ++s.first;
            auto* sym = intern("quote");
            return cons(sym, cons(read(s), nullptr));
        } else {
            auto old_first = s.first;
            s.skip_until_the_next_white_space();
            return string_view(old_first, s.first, s.orig).read_atom();
        }
    }
    return nullptr;
}

void lisp::Environment::write(std::ostream& out) const {
    for (auto p = this; p; p = p->parent) {
        p->vars->write(out);
    }
}

void lisp::Integer::write(std::ostream& out) const {
    out << value;
}

void lisp::Boolean::write(std::ostream& out) const {
    out << (value ? "#t" : "#f");
}

void lisp::Cell::write(std::ostream& out) const {
    out << '(';
    bool first = true;
    for (const Object* obj=this; obj; obj = obj->cdr) {
        if (!first) { out << ' '; } else { first = false; }
        out << obj->car;
        if (obj->cdr && obj->cdr->type != Type::Cell) {
            out << " . " << obj->cdr;
            break;
        }
    }
    out << ')';
}


void lisp::Object::write(std::ostream& out) const {
    out << type;
}

void lisp::Symbol::write(std::ostream& out) const {
    out << name;
}

void lisp::Primitive::write(std::ostream& out) const {
    dl::symbol s(reinterpret_cast<void*>(this->function));
    if (s) {
        out << s.name();
    } else {
        out << "<primitive>";
    }
}

void lisp::Function::write(std::ostream& out) const {
    switch (type) {
        case Type::Function: out << "<function>"; break;
        case Type::Macro: out << "<macro>"; break;
        default: error("Bug: print: Unknown tag type: ", type);
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

std::string lisp::to_string(const Object* obj) {
    std::stringstream tmp;
    tmp << obj;
    return tmp.str();
}

static size_t list_length(const Object* list) {
    size_t len = 0;
    for (;;) {
        if (list == nullptr)
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
auto progn(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list == nullptr) { return {nullptr,nullptr}; }
    if (list->cdr == nullptr) { return list->car->eval(kernel, env); }
    auto child = sbn::make_pointer<Progn>(env, list);
    auto ptr = child.get();
    sbn::upstream<sbn::Local>(kernel, std::move(child));
    return {list, ptr};
}

// Evaluates all the list elements and returns their return values as a new list.
static Object* eval_list(Kernel* kernel, Environment* env, Object* list) {
    Object* head = nullptr;
    Object* tail = nullptr;
    for (Object* lp = list; lp != nullptr; lp = lp->cdr) {
        Object* tmp = lp->car->eval(kernel, env).object;
        if (!tmp) {
            log("Bug: eval_list element is null");
        }
        if (head == nullptr) {
            head = tail = cons(tmp, nullptr);
        } else {
            tail->cdr = cons(tmp, nullptr);
            tail = tail->cdr;
        }
    }
    if (head == nullptr)
        return nullptr;
    return head;
}

static bool is_list(Object* obj) {
    return obj == nullptr || obj->type == Type::Cell;
}

// Apply fn with args.
auto apply(Kernel* kernel, Environment* env, Object* obj, Object* fn, Object* args) -> Result {
    if (!is_list(args)) {
        error("argument must be a list");
    }
    if (fn->type == Type::Primitive) {
        auto tmp = reinterpret_cast<Primitive*>(fn);
        return tmp->function(kernel, env, args);
    }
    if (fn->type == Type::Function) {
        auto child = sbn::make_pointer<Apply>(env, obj, cons(fn,args));
        auto ptr = child.get();
        sbn::upstream<sbn::Local>(kernel, std::move(child));
        return {obj,ptr};
    }
    error("not supported");
    return {};
}

Object* lisp::Environment::find(Symbol* symbol) {
    for (Environment* p = this; p; p = p->parent) {
        for (Object* cell = p->vars; cell != nullptr; cell = cell->cdr) {
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
    return progn(kernel, newenv, body).object;
}

Object* lisp::make(Type type) {
    switch (type) {
        case Type::Integer: return new Integer;
        case Type::Cell: return new Cell;
        case Type::Symbol: return new Symbol;
        case Type::Primitive: return new Primitive;
        case Type::Function: return new Function(type);
        case Type::Macro: return new Function(type);
        case Type::Environment: return new Environment(nullptr, nullptr);
        case Type::Boolean: return new Boolean;
        default: return nullptr;
    }
}

void lisp::Object::write(sbn::kernel_buffer& out) const {
    out.write(this->type);
}

Object* lisp::read(sbn::kernel_buffer& in) {
    Type type{};
    in.read(type);
    if (type == Type::Void) { return nullptr; }
    if (type == Type::Symbol) {
        std::string name;
        in.read(name);
        return intern(name);
    }
    auto result = make(type);
    result->read(in);
    return result;
}

void lisp::Object::read(sbn::kernel_buffer& in) {}

void lisp::Cell::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out << this->car;
    out << this->cdr;
}

void lisp::Cell::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in >> this->car;
    in >> this->cdr;
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
    this->function = result->second;
}

void lisp::Function::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out << this->params;
    out << this->body;
    out << this->env;
}

void lisp::Function::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    {
        Object* tmp{};
        in >> tmp;
        this->params = cast<Cell>(tmp);
    }
    in >> this->body;
    {
        Object* tmp{};
        in >> tmp;
        this->env = cast<Environment>(tmp);
    }
}

void lisp::Integer::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->value);
}

void lisp::Integer::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    in.read(this->value);
}

void lisp::Boolean::write(sbn::kernel_buffer& out) const {
    lisp::Object::write(out);
    out.write(this->value);
}

void lisp::Boolean::read(sbn::kernel_buffer& in) {
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
    if (this->parent) {
        out.write(true);
        this->parent->write(out);
    } else {
        out.write(false);
    }
}

void lisp::Environment::read(sbn::kernel_buffer& in) {
    lisp::Object::read(in);
    this->vars = ::lisp::read(in);
    bool has_parent = false;
    in.read(has_parent);
    if (has_parent) {
        this->parent = cast<Environment>(::lisp::read(in));
    } else {
        this->parent = nullptr;
    }
}

auto lisp::Object::eval(Kernel* kernel, Environment* env) -> Result {
    log("eval _ => _", this, this);
    return this;
}

auto lisp::Symbol::eval(Kernel* kernel, Environment* env) -> Result {
    Object* bind = env->find(this);
    if (!bind) {
        error("Undefined symbol: ", this->name.data());
    }
    auto result = bind->cdr;
    log("eval _ => _", this, result);
    return result;
}

auto lisp::Cell::eval(Kernel* kernel, Environment* env) -> Result {
    // Function application form
    Object* expanded = macroexpand(kernel, env, this);
    Result result;
    if (expanded != this) {
        result = expanded->eval(kernel, env);
    } else {
        auto res = this->car->eval(kernel, env);
        Assert(!res.asynchronous());
        auto fn = res.object;
        Object* args = this->cdr;
        if (fn->type != Type::Primitive && fn->type != Type::Function) {
            log("head _", fn);
            error("The head of a list must be a function");
        }
        result = apply(kernel, env, this, fn, args);
    }
    log("eval _ => _", this, result);
    return result;
}

bool lisp::Object::equals(Object* rhs) const {
    Expects(rhs);
    return this->type == rhs->type;
}

bool lisp::Integer::equals(Object* rhs) const {
    Expects(rhs);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Integer*>(rhs));
}

bool lisp::Boolean::equals(Object* rhs) const {
    Expects(rhs);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Boolean*>(rhs));
}

bool lisp::Primitive::equals(Object* rhs) const {
    Expects(rhs);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Primitive*>(rhs));
}

bool lisp::Symbol::equals(Object* rhs) const {
    Expects(rhs);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Symbol*>(rhs));
}

bool lisp::Cell::equals(Object* rhs) const {
    Expects(rhs);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Cell*>(rhs));
}

bool lisp::Function::equals(Object* rhs) const {
    Expects(rhs);
    Expects(params);
    Expects(body);
    Expects(env);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Function*>(rhs));
}

bool lisp::Environment::equals(Object* rhs) const {
    Expects(rhs);
    Expects(vars);
    return Object::equals(rhs) && operator==(*reinterpret_cast<Environment*>(rhs));
}

//======================================================================
// Functions and special forms
//======================================================================

// 'expr
auto prim_quote(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list_length(list) != 1)
        error("Malformed quote");
    return list->car;
}

// (list expr ...)
auto prim_list(Kernel* kernel, Environment* env, Object* list) -> Result {
    return eval_list(kernel, env, list);
}

// (setq <symbol> expr)
auto prim_setq(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list_length(list) != 2 || list->car->type != Type::Symbol) {
        error("Malformed setq");
    }
    auto tmp = reinterpret_cast<Symbol*>(list->car);
    Object* bind = env->find(tmp);
    if (!bind) {
        error("Unbound variable ", tmp->name.data());
    }
    Object* value = list->cdr->car->eval(kernel, env).object;
    bind->cdr = value;
    return value;
}

// (+ <integer> ...)
auto prim_plus(Kernel* kernel, Environment* env, Object* list) -> Result {
    int sum = 0;
    for (Object* args = eval_list(kernel, env, list); args != nullptr; args = args->cdr) {
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
    for (Object* p = list->car; p != nullptr; p = p->cdr) {
        if (p->car->type != Type::Symbol) {
            error("Parameter must be a symbol");
        }
        if (!is_list(p->cdr)) {
            error("Parameter list is not a flat list");
        }
    }
    auto params = lisp::cast<Cell>(list->car);
    Object* cdr = list->cdr;
    return make_function(type, params, cdr, env);
}

// (lambda (<symbol> ...) expr ...)
Result prim_lambda(Kernel* kernel, Environment* env, Object* list) {
    return handle_function(kernel, env, list, Type::Function);
}

Object* handle_defun(Kernel* kernel, Environment* env, Object* list, Type type) {
    if (list->car->type != Type::Symbol || list->cdr->type != Type::Cell)
        error("Malformed defun");
    auto* sym = reinterpret_cast<Symbol*>(list->car);
    Object* rest = list->cdr;
    Object* fn = handle_function(kernel, env, rest, type);
    env->add(sym, fn);
    return fn;
}

// (defun <symbol> (<symbol> ...) expr ...)
auto prim_defun(Kernel* kernel, Environment* env, Object* list) -> Result {
    return handle_defun(kernel, env, list, Type::Function);
}

// (define <symbol> expr)
auto prim_define(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list_length(list) != 2 || list->car->type != Type::Symbol)
        error("Malformed define");
    auto* sym = reinterpret_cast<Symbol*>(list->car);
    Object* value = list->cdr->car->eval(kernel, env).object;
    env->add(sym, value);
    return value;
}

// (defmacro <symbol> (<symbol> ...) expr ...)
auto prim_defmacro(Kernel* kernel, Environment* env, Object* list) -> Result {
    return handle_defun(kernel, env, list, Type::Macro);
}

// (macroexpand expr)
auto prim_macroexpand(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list_length(list) != 1)
        error("Malformed macroexpand");
    Object* body = list->car;
    return macroexpand(kernel, env, body);
}

// (println expr)
Result prim_println(Kernel* kernel, Environment* env, Object* list) {
    std::cout << list->car->eval(kernel, env) << '\n';
    return {};
}

// (if expr expr expr ...)
auto prim_if(Kernel* kernel, Environment* env, Object* list) -> Result {
    if (list_length(list) < 2)
        error("Malformed if");
    auto res = list->car->eval(kernel, env);
    Assert(!res.asynchronous());
    auto cond = res.object;
    if (cond != nullptr) {
        Object* then = list->cdr->car;
        return then->eval(kernel, env);
    }
    Object* els = list->cdr->cdr;
    return els == nullptr ? Result{} : progn(kernel, env, els);
}

// (= <integer> <integer>)
Result prim_num_eq(Kernel* kernel, Environment* env, Object* list) {
    if (list_length(list) != 2)
        error("Malformed =");
    Object* values = eval_list(kernel, env, list);
    Object* x = values->car;
    Object* y = values->cdr->car;
    if (x->type != Type::Integer || y->type != Type::Integer)
        error("= only takes numbers");
    auto a = reinterpret_cast<Integer*>(x);
    auto b = reinterpret_cast<Integer*>(y);
    return a->value == b->value ? True : nullptr;
}

// (exit)
Result prim_exit(Kernel* kernel, Environment* env, Object* list) {
    exit(0);
    return {};
}

auto prim_cons(Kernel* kernel, Environment* env, Object* list) -> Result {
    auto child = sbn::make_pointer<Cons>(env, list);
    auto ptr = child.get();
    sbn::upstream<sbn::Local>(kernel, std::move(child));
    return {list, ptr};
}

Result prim_car(Kernel* kernel, Environment* env, Object* list) {
    /*
    auto result = list->car->eval(kernel, env);
    Assert(!result.asynchronous());
    return result.object->car;
    */
    auto child = sbn::make_pointer<Car>(env, list);
    auto ptr = child.get();
    sbn::upstream<sbn::Local>(kernel, std::move(child));
    return {ptr->expression(), ptr};
}

Result prim_cdr(Kernel* kernel, Environment* env, Object* list) {
    auto result = list->car->eval(kernel, env);
    Assert(!result.asynchronous());
    return result.object->cdr;
}

Result prim_is_null(Kernel* kernel, Environment* env, Object* list) {
    return list == nullptr ? True : False;
}

Result prim_equal(Kernel* kernel, Environment* env, Object* list) {
    auto a = list->car->eval(kernel, env);
    Assert(!a.asynchronous());
    auto b = list->cdr->car->eval(kernel, env);
    Assert(!b.asynchronous());
    return a.object->equals(b.object) ? True : False;
}

void lisp::define(Environment* env, const char* name, Object* value) {
    env->add(intern(name), value);
}

void lisp::define(Environment* env, const char* name, Cpp_function function, Cpp_function_id id) {
    {
        std::lock_guard<std::mutex> lock(global_mutex);
        functions[id] = function;
        /*
        auto result = functions.emplace(id, function);
        if (!result.second) {
            std::clog << "id=" << id << std::endl;
            throw std::invalid_argument("duplicate function id");
        }
        */
    }
    env->add(intern(name), new Primitive(function,id));
}

static void define_constants(Environment* env) {
    env->add(lisp::intern("#t"), True);
    env->add(lisp::intern("#f"), False);
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
    define(env, "equal?", prim_equal, 17);
    define(env, "sleep", [] (Kernel* kernel, Environment* env, List* args) -> Result {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return {};
    }, 1000);
}

void lisp::init() {
    Dot = new Symbol(".");
    True = new Boolean(true);
    False = new Boolean(false);
    Async = new Object(Type::Async);
    ::Symbols = nullptr;
}

Environment* lisp::top_environment() {
    std::unique_lock<std::mutex> lock(::global_mutex);
    if (!Top) {
        Top = new Environment();
        lock.unlock();
        define_constants(Top);
        define_primitives(Top);
    }
    return Top;
}

auto lisp::Allocator::allocate(size_type n) -> pointer {
    if (this->_pages.empty()) {
        this->_pages.emplace_back(4096);
    }
    if (this->_pages.back().size-this->_offset < n) {
        this->_pages.emplace_back(std::max(size_type(4096), n));
        this->_offset = 0;
        return this->_pages.back().data;
    }
    auto ret = reinterpret_cast<pointer>(
         reinterpret_cast<size_type>(this->_pages.back().data) + this->_offset);
    this->_offset += n;
    return ret;
}

lisp::Allocator::Page::Page(size_type n):
data(::mmap(nullptr, n, PROT_READ|PROT_WRITE, MAP_PRIVATE, -1, 0)),
size(n) {
    if (!data) { throw std::bad_alloc(); }
}

lisp::Allocator::Page::~Page() noexcept {
    int ret = ::munmap(this->data, this->size);
    if (ret == -1) { std::terminate(); }
}
