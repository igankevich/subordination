#ifndef SUBORDINATION_MINILISP_MINILISP_HH
#define SUBORDINATION_MINILISP_MINILISP_HH

#include <cstdint>
#include <iosfwd>

namespace lisp {

    class Environment;
    class Kernel;
    class List;
    class Object;
    class Special;
    class Symbol;

    enum class Type {
        Integer = 1,
        Cell = 2,
        Symbol = 3,
        Primitive = 4,
        Function = 5,
        Macro = 6,
        Special = 7,
        Environment = 8,
    };

    const char* to_string(Type t) noexcept;
    std::ostream& operator<<(std::ostream& out, Type rhs);
    Object* make(Type type);

    enum class Special_type {
        Nil = 1,
        Dot = 2,
        Cparen = 3,
        True = 4,
    };

    const char* to_string(Special_type t) noexcept;
    std::ostream& operator<<(std::ostream& out, Special_type rhs);
    Special* make(Special_type type) noexcept;

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
            Object* car;
            Object* cdr;
            Object* unused;
        };

        inline explicit Object(Type t) noexcept: type(t) {}
        virtual void write(std::ostream& out) const = 0;
        virtual void write(sbn::kernel_buffer& out) const;
        virtual void read(sbn::kernel_buffer& in);
        virtual Object* eval(Kernel* kernel, Environment* env);

        Object() = default;
        virtual ~Object() = default;
        Object(const Object&) = default;
        Object& operator=(const Object&) = default;
        Object(Object&&) = default;
        Object& operator=(Object&&) = default;

    };

    std::ostream& operator<<(std::ostream& out, const Object* obj);
    Object* read(sbn::kernel_buffer& in);

    class Special: public Object {

    public:
        Special_type subtype{};

    public:
        inline explicit Special(Special_type t): Object(Type::Special), subtype(t) {}
        inline Special(): Object(Type::Special) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        ~Special() = default;
        Special(const Special&) = default;
        Special& operator=(const Special&) = default;
        Special(Special&&) = default;
        Special& operator=(Special&&) = default;

    };

    class Cell: public Object {
    public:
        //Object* car{};
        //Object* cdr{};
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        Object* eval(Kernel* kernel, Environment* env) override;
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

        ~Primitive() = default;
        Primitive(const Primitive&) = default;
        Primitive& operator=(const Primitive&) = default;
        Primitive(Primitive&&) = default;
        Primitive& operator=(Primitive&&) = default;

    };

    class Function: public Object {
    public:
        Symbol* params;
        Object* body;
        Environment* env;

        inline explicit Function(Type t): Object(t) {}
        inline Function(): Object(Type::Function) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        ~Function() = default;
        Function(const Function&) = default;
        Function& operator=(const Function&) = default;
        Function(Function&&) = default;
        Function& operator=(Function&&) = default;
    };

    class Integer: public Object {
    public:
        int value = 0;
    public:
        inline explicit Integer(int v): Object(Type::Integer), value(v) {}
        inline Integer(): Object(Type::Integer) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        ~Integer() = default;
        Integer(const Integer&) = default;
        Integer& operator=(const Integer&) = default;
        Integer(Integer&&) = default;
        Integer& operator=(Integer&&) = default;
    };

    class Symbol: public Object {
    public:
        std::string name;
    public:
        inline Symbol(std::string n): Object(Type::Symbol), name(std::move(n)) {}
        inline Symbol(): Object(Type::Symbol) {}
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;
        Object* eval(Kernel* kernel, Environment* env) override;

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
        Environment(Symbol* symbols, Object* values, Environment* parent);

        Object* find(Symbol* symbol);
        void add(Symbol* symbol, Object* value);
        void write(std::ostream& out) const override;
        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        ~Environment() = default;
        Environment(const Environment&) = default;
        Environment& operator=(const Environment&) = default;
        Environment(Environment&&) = default;
        Environment& operator=(Environment&&) = default;
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

    template <class Target> inline Target*
    cast(Object* object) {
        auto ptr = dynamic_cast<Target*>(object);
        if (!ptr) { throw std::bad_cast(); }
        return ptr;
    }

}

#endif // vim:filetype=cpp
