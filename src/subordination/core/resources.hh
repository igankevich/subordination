#ifndef SUBORDINATION_CORE_RESOURCES_HH
#define SUBORDINATION_CORE_RESOURCES_HH

#include <memory>

#include <unistdx/base/byte_buffer>

namespace sbn {

    /// \brief Mini-language for selecting nodes with appropriate resources
    /// (memory, cores, graphical accelerators etc.)
    namespace resources {

        class Any {
        public:
            enum class Type: uint8_t {Boolean=0, U8=1, U16=2, U32=3, U64=4};
        private:
            union {
                bool _b;
                uint8_t _u8;
                uint16_t _u16;
                uint32_t _u32;
                uint64_t _u64;
            };
            Type _type{};
        public:
            inline Any(bool value) noexcept: _b{value}, _type{Type::Boolean} {}
            inline Any(uint8_t value) noexcept: _u8{value}, _type{Type::U8} {}
            inline Any(uint16_t value) noexcept: _u16{value}, _type{Type::U16} {}
            inline Any(uint32_t value) noexcept: _u32{value}, _type{Type::U32} {}
            inline Any(uint64_t value) noexcept: _u64{value}, _type{Type::U64} {}
            inline Type type() const noexcept { return this->_type; }

            template <class T>
            inline T cast() const noexcept {
                switch (type()) {
                    case Any::Type::Boolean: return static_cast<T>(this->_b);
                    case Any::Type::U8: return static_cast<T>(this->_u8);
                    case Any::Type::U16: return static_cast<T>(this->_u16);
                    case Any::Type::U32: return static_cast<T>(this->_u32);
                    case Any::Type::U64: return static_cast<T>(this->_u64);
                    default: return T{};
                }
            }

            void write(sys::byte_buffer& out) const;
            void read(sys::byte_buffer& in);

            Any() = default;
            ~Any() = default;
            Any(const Any&) = default;
            Any& operator=(const Any&) = default;
            Any(Any&&) = default;
            Any& operator=(Any&&) = default;
        };

        template <class T>
        inline T cast(const Any& a) noexcept { return a.cast<T>(); }

        /*
        template <Any::Type t> struct any_to_cpp_type {};
        template <> struct any_to_cpp_type<Any::Type::Boolean> { using type = bool; };
        template <> struct any_to_cpp_type<Any::Type::U8> { using type = uint8_t; };
        template <> struct any_to_cpp_type<Any::Type::U16> { using type = uint16_t; };
        template <> struct any_to_cpp_type<Any::Type::U32> { using type = uint32_t; };
        template <> struct any_to_cpp_type<Any::Type::U64> { using type = uint64_t; };
        template <class T> struct cpp_to_any_type {};
        template <> struct cpp_to_any_type<bool> { static constexpr const auto value = Any::Type::Boolean; };
        template <> struct cpp_to_any_type<uint8_t> { static constexpr const auto value = Any::Type::U8; };
        template <> struct cpp_to_any_type<uint16_t> { static constexpr const auto value = Any::Type::U16; };
        template <> struct cpp_to_any_type<uint32_t> { static constexpr const auto value = Any::Type::U32; };
        template <> struct cpp_to_any_type<uint64_t> { static constexpr const auto value = Any::Type::U64; };
        */

        enum class Resource {
        };

        class Context {
        public:
            virtual Any get(Resource resource) const noexcept;
            Context() = default;
            virtual ~Context() = default;
            Context(const Context&) = delete;
            Context& operator=(const Context&) = delete;
            Context(Context&&) = delete;
            Context& operator=(Context&&) = delete;
        };

        class Expression {
        public:
            Expression() = default;
            virtual ~Expression() = default;
            Expression(const Expression&) = delete;
            Expression& operator=(const Expression&) = delete;
            Expression(Expression&&) = delete;
            Expression& operator=(Expression&&) = delete;
            virtual Any evaluate(Context* context) const noexcept = 0;
            virtual void write(sys::byte_buffer& out) const = 0;
            virtual void read(sys::byte_buffer& in) = 0;
        };

        using expression_ptr = std::unique_ptr<Expression>;

        class Symbol: public Expression {
        private:
            Resource _name{};
        public:
            inline explicit Symbol(Resource name) noexcept: _name(name) {}
            Any evaluate(Context* context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            Symbol() = default;
            ~Symbol() = default;
            Symbol(const Symbol&) = delete;
            Symbol& operator=(const Symbol&) = delete;
            Symbol(Symbol&&) = delete;
            Symbol& operator=(Symbol&&) = delete;
        };

        class Constant: public Expression {
        private:
            Any _value{};
        public:
            inline explicit Constant(Any value) noexcept: _value(value) {}
            Any evaluate(Context* context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            Constant() = default;
            ~Constant() = default;
            Constant(const Constant&) = delete;
            Constant& operator=(const Constant&) = delete;
            Constant(Constant&&) = delete;
            Constant& operator=(Constant&&) = delete;
        };

        class Not: public Expression {
        private:
            expression_ptr _arg;
        public:
            inline explicit Not(expression_ptr&& arg) noexcept: _arg(std::move(arg)) {}
            Any evaluate(Context* context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            Not() = default;
            ~Not() = default;
            Not(const Not&) = delete;
            Not& operator=(const Not&) = delete;
            Not(Not&&) = delete;
            Not& operator=(Not&&) = delete;
        };

        #define SBN_RESOURCES_BINARY_OPERATION(NAME) \
            class NAME: public Expression { \
            private: \
                expression_ptr _a, _b; \
            public: \
                inline explicit NAME(expression_ptr&& a, expression_ptr&& b) noexcept: \
                _a(std::move(a)), _b(std::move(b)) {} \
                Any evaluate(Context* context) const noexcept override; \
                void write(sys::byte_buffer& out) const override; \
                void read(sys::byte_buffer& in) override; \
                NAME() = default; \
                ~NAME() = default; \
                NAME(const NAME&) = delete; \
                NAME& operator=(const NAME&) = delete; \
                NAME(NAME&&) = delete; \
                NAME& operator=(NAME&&) = delete; \
            };

        SBN_RESOURCES_BINARY_OPERATION(And);
        SBN_RESOURCES_BINARY_OPERATION(Or);
        SBN_RESOURCES_BINARY_OPERATION(Xor);
        SBN_RESOURCES_BINARY_OPERATION(Less_than);
        SBN_RESOURCES_BINARY_OPERATION(Less_or_equal);
        SBN_RESOURCES_BINARY_OPERATION(Equal);
        SBN_RESOURCES_BINARY_OPERATION(Greater_than);
        SBN_RESOURCES_BINARY_OPERATION(Greater_or_equal);

        #undef SBN_RESOURCES_BINARY_OPERATION

        expression_ptr read(sys::byte_buffer& in);

        enum class Expressions: uint8_t {
            Symbol=1,
            Constant=2,
            Not=3,
            And=4,
            Or=5,
            Xor=6,
            Less_than=7,
            Less_or_equal=8,
            Equal=9,
            Greater_than=10,
            Greater_or_equal=11,
        };

        expression_ptr make_expression(Expressions type);

    }

}

#endif // vim:filetype=cpp
