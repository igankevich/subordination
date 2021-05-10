#ifndef SUBORDINATION_MINILISP_MINILISP_HH
#define SUBORDINATION_MINILISP_MINILISP_HH

#include <cstdint>
#include <iosfwd>

#include <subordination/bits/contracts.hh>
#include <subordination/core/kernel.hh>

namespace lisp {

    class Environment;
    class Kernel;
    class List;
    class Object;
    class Symbol;
    struct string_view;

    enum class Type {
        Void = 0,
        Integer = 1,
        Cell = 2,
        Symbol = 3,
        Primitive = 4,
        Function = 5,
        Macro = 6,
        Environment = 7,
        Boolean = 8,
        Async = 9,
    };

    const char* to_string(Type t) noexcept;
    std::ostream& operator<<(std::ostream& out, Type rhs);
    Object* make(Type type);

    struct string_view {
        using value_type = char;
        using const_iterator = const value_type*;
        using iterator = const_iterator;
        using size_type = size_t;
        iterator first{};
        iterator last{};
        iterator orig{};
        inline string_view(const char* s) noexcept:
        first(s), last(s+std::char_traits<char>::length(s)), orig(s) {}
        inline string_view(const std::string& s) noexcept:
        first(s.data()), last(s.data()+s.size()), orig(s.data()) {}
        inline string_view(iterator a, iterator b) noexcept: first(a), last(b), orig(a) {}
        inline string_view(iterator a, iterator b, iterator orig) noexcept:
        first(a), last(b), orig(orig) {}
        inline iterator data() const noexcept { return this->first; }
        inline size_type size() const noexcept { return this->last-this->first; }
        inline bool empty() const noexcept { return this->first == this->last; }
        inline char front() const noexcept { return *this->first; }
        void trim();
        void trim_left();
        void skip_until_the_end_of_the_line();
        void skip_until_the_next_white_space();
        iterator scan_for_matching_bracket() const;
        Object* read_list();
        Object* read_atom();
        bool is_integer() const noexcept;
        bool is_symbol() const noexcept;

        bool operator==(string_view rhs) const noexcept;

        inline bool operator!=(string_view rhs) const noexcept {
            return !this->operator==(rhs);
        }

        string_view() = default;
        ~string_view() = default;
        string_view(const string_view&) = default;
        string_view& operator=(const string_view&) = default;
        string_view(string_view&&) = default;
        string_view& operator=(string_view&&) = default;

    };

    std::ostream& operator<<(std::ostream& out, const string_view& rhs);

    using Cpp_function = Object* (*)(Kernel* kernel, Environment* env, Object* args);
    using Cpp_function_id = std::uint64_t;
    using Function2 = Object* (*)(Kernel* kernel, Environment* env, List* args);

    class Object {
    public:
        // The first word of the object represents the type of the object. Any code that handles object
        // needs to check its type first, then access the following union members.
        Type type{};

        // Cell
        struct {
            Object* car = nullptr;
            Object* cdr = nullptr;
            Object* unused;
        };

        inline explicit Object(Type t) noexcept: type(t) {}
        virtual void write(std::ostream& out) const;
        virtual void write(sbn::kernel_buffer& out) const;
        virtual void read(sbn::kernel_buffer& in);
        virtual Object* eval(Kernel* kernel, Environment* env);
        virtual bool equals(Object* rhs) const;
        inline bool empty() const noexcept { return this->cdr == nullptr; }

        Object() = default;
        virtual ~Object() = default;
        Object(const Object&) = default;
        Object& operator=(const Object&) = default;
        Object(Object&&) = default;
        Object& operator=(Object&&) = default;

    };

    std::ostream& operator<<(std::ostream& out, const Object* obj);
    std::string to_string(const Object* obj);
    Object* read(sbn::kernel_buffer& in);

    inline sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, Object*& rhs) {
        rhs = read(in);
        return in;
    }

    inline sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const Object* obj) {
        if (obj) { obj->write(out); } else { out.write(Type::Void); }
        return out;
    }

    class Cell: public Object {
    public:
        //Object* car{};
        //Object* cdr{};
        inline explicit Cell(Object* a, Object* b): Object(Type::Cell) { car = a, cdr = b; }
        inline Cell(): Object(Type::Cell) {}

        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        Object* eval(Kernel* kernel, Environment* env) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Cell& rhs) const noexcept {
            return
                ((this->car && rhs.car && this->car->equals(rhs.car)) ||
                 (!this->car && !rhs.car)) &&
                ((this->cdr && rhs.cdr && this->cdr->equals(rhs.cdr)) ||
                 (!this->cdr && !rhs.cdr));
        }

        inline bool operator!=(const Cell& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Cell() = default;
        Cell(const Cell&) = default;
        Cell& operator=(const Cell&) = default;
        Cell(Cell&&) = default;
        Cell& operator=(Cell&&) = default;
    };

    class Primitive: public Object {

    public:
        Cpp_function fn{};
        Cpp_function_id _id{};

    public:
        inline explicit Primitive(Cpp_function f, Cpp_function_id id):
        Object(Type::Primitive), fn(f), _id(id) {}
        inline Primitive(): Object(Type::Primitive) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Primitive& rhs) const noexcept {
            return this->_id == rhs._id && this->fn == rhs.fn;
        }

        inline bool operator!=(const Primitive& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Primitive() = default;
        Primitive(const Primitive&) = default;
        Primitive& operator=(const Primitive&) = default;
        Primitive(Primitive&&) = default;
        Primitive& operator=(Primitive&&) = default;

    };

    class Integer: public Object {
    public:
        int64_t value = 0;
    public:
        inline explicit Integer(int64_t v): Object(Type::Integer), value(v) {}
        inline Integer(): Object(Type::Integer) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Integer& rhs) const noexcept {
            return this->value == rhs.value;
        }

        inline bool operator!=(const Integer& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Integer() = default;
        Integer(const Integer&) = default;
        Integer& operator=(const Integer&) = default;
        Integer(Integer&&) = default;
        Integer& operator=(Integer&&) = default;
    };

    class Boolean: public Object {
    public:
        bool value = false;
    public:
        inline explicit Boolean(bool v): Object(Type::Boolean), value(v) {}
        inline Boolean(): Object(Type::Boolean) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Boolean& rhs) const noexcept {
            return this->value == rhs.value;
        }

        inline bool operator!=(const Boolean& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Boolean() = default;
        Boolean(const Boolean&) = default;
        Boolean& operator=(const Boolean&) = default;
        Boolean(Boolean&&) = default;
        Boolean& operator=(Boolean&&) = default;
    };

    class Symbol: public Object {
    public:
        std::string name;
    public:
        inline Symbol(string_view s): Object(Type::Symbol), name(s.first, s.last) {}
        inline Symbol(): Object(Type::Symbol) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        Object* eval(Kernel* kernel, Environment* env) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Symbol& rhs) const noexcept {
            return this == std::addressof(rhs) || this->name == rhs.name;
        }

        inline bool operator!=(const Symbol& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Symbol() = default;
        Symbol(const Symbol&) = default;
        Symbol& operator=(const Symbol&) = default;
        Symbol(Symbol&&) = default;
        Symbol& operator=(Symbol&&) = default;
    };

    class Environment: public Object {
    private:
        Object* vars{};
        Environment* parent{};
    public:
        inline Environment(): Object(Type::Environment) {}
        inline Environment(Object* variables, Environment* parent) {
            this->type = Type::Environment;
            this->vars = variables;
            this->parent = parent;
        }
        Environment(Cell* symbols, Object* values, Environment* parent);

        Object* find(Symbol* symbol);
        void add(Symbol* symbol, Object* value);
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Environment& rhs) const noexcept {
            return this->vars->equals(rhs.vars) &&
                ((this->parent && rhs.parent && this->parent->equals(rhs.parent)) ||
                 (!this->parent && !rhs.parent));
        }

        inline bool operator!=(const Environment& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Environment() = default;
        Environment(const Environment&) = default;
        Environment& operator=(const Environment&) = default;
        Environment(Environment&&) = default;
        Environment& operator=(Environment&&) = default;
    };

    class Function: public Object {
    public:
        Cell* params;
        Object* body;
        Environment* env;

        inline explicit Function(Cell* params, Object* body, Environment* env):
        Object(Type::Function) { this->params = params, this->body = body, this->env = env; }
        inline explicit Function(Type t): Object(t) {}
        inline Function(): Object(Type::Function) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        bool equals(Object* rhs) const override;

        inline bool operator==(const Function& rhs) const noexcept {
            return this->params->equals(rhs.params) &&
                this->body->equals(rhs.body) &&
                this->env->equals(rhs.env);
        }

        inline bool operator!=(const Function& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        ~Function() = default;
        Function(const Function&) = default;
        Function& operator=(const Function&) = default;
        Function(Function&&) = default;
        Function& operator=(Function&&) = default;
    };

    class List: public Object {
    public:
        inline Object* head() noexcept { return this->car; }
        inline const Object* head() const noexcept { return this->car; }
        inline Object* tail() noexcept { return this->cdr; }
        inline const Object* tail() const noexcept { return this->cdr; }
        size_t size() const;
    };

    void define(Environment* env, const char* name, Cpp_function function, Cpp_function_id id);
    void define(Environment* env, const char* name, Object* value);

    inline void define(Environment* env, const char* name, Function2 function,
                       Cpp_function_id function_id) {
        define(env, name, reinterpret_cast<Cpp_function>(function), function_id);
    }

    inline Object* eval(Kernel* kernel, Environment* env, Object* obj) {
        return obj->eval(kernel,env);
    }

    Cell* cons(Object* car, Object* cdr);
    Object* equal(Object* a, Object* b);

    inline bool to_boolean(Object* object) noexcept {
        Expects(object);
        return object->type == Type::Boolean && reinterpret_cast<Boolean*>(object)->value;
    }

    template <class Target> inline Target*
    cast(Object* object) {
        Expects(object);
        auto ptr = dynamic_cast<Target*>(object);
        if (!ptr) { throw std::bad_cast(); }
        return ptr;
    }

    Object* read(const char* string);
    Object* read(string_view& s);
    Environment* top_environment();
    void init();
    Symbol* intern(lisp::string_view name);

    class Kernel: public sbn::kernel {

    private:
        Environment* _environment{};
        Object* _expression{};
        Object* _result = nullptr;
        size_t _index = 0;

    public:
        Kernel() = default;
        inline Kernel(Environment* env, Object* expr): Kernel(env, expr, 0) {}
        inline Kernel(Environment* env, Object* expr, size_t index):
        _environment(env), _expression(expr), _index(index) {}

        void act() override;
        void react(sbn::kernel_ptr&& k) override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

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

    class Main: public Kernel {

    private:
        std::string _current_script;
        string_view _view;

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

        void act() override;
        void react(sbn::kernel_ptr&& k) override;

    private:
        void read_eval();
        void print();

    };

    class Allocator {

    public:
        using pointer = void*;
        using size_type = std::size_t;

    public:
        struct Page {
            pointer data;
            size_type size;
            explicit Page(size_type n);
            ~Page() noexcept;
        };

    private:
        std::vector<Page> _pages;
        // Current page offset.
        size_type _offset = 0;

    public:
        auto allocate(size_type n) -> pointer;
    };

}

#endif // vim:filetype=cpp
