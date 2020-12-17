#ifndef SUBORDINATION_CORE_RESOURCES_HH
#define SUBORDINATION_CORE_RESOURCES_HH

#include <array>
#include <memory>
#include <ostream>
#include <type_traits>

#include <unistdx/base/byte_buffer>

namespace sbn {

    /// \brief Mini-language for selecting nodes with appropriate resources
    /// (memory, cores, graphical accelerators etc.)
    namespace resources {

        class Any {
        public:
            enum class Type: uint8_t {Boolean=0, U8=1, U16=2, U32=3, U64=4, String=5};
        private:
            union {
                bool _b;
                uint8_t _u8;
                uint16_t _u16;
                uint32_t _u32;
                uint64_t _u64;
                char* _string;
            };
            Type _type{};
        public:
            inline Any(bool value) noexcept: _b{value}, _type{Type::Boolean} {}
            inline Any(uint8_t value) noexcept: _u8{value}, _type{Type::U8} {}
            inline Any(uint16_t value) noexcept: _u16{value}, _type{Type::U16} {}
            inline Any(uint32_t value) noexcept: _u32{value}, _type{Type::U32} {}
            inline Any(uint64_t value) noexcept: _u64{value}, _type{Type::U64} {}
            Any(const char* s, size_t n);
            inline Any(const char* s): Any{s, std::char_traits<char>::length(s)} {}
            inline Type type() const noexcept { return this->_type; }

            inline bool boolean() const noexcept {
                if (this->_type != Type::Boolean) { return false; }
                return this->_b;
            }

            inline uint64_t unsigned_integer() const noexcept {
                if (this->_type != Type::U64) { return 0; }
                return this->_u64;
            }

            void write(sys::byte_buffer& out) const;
            void write(std::ostream& out) const;
            void read(sys::byte_buffer& in);

            void swap(Any& rhs) noexcept;
            Any() = default;
            ~Any() noexcept;
            Any(const Any&);
            Any& operator=(const Any&);
            inline Any(Any&& rhs) noexcept { swap(rhs); }
            inline Any& operator=(Any&& rhs) noexcept { swap(rhs); return *this; }
        };

        inline void swap(Any& a, Any& b) noexcept { a.swap(b); }

        std::ostream& operator<<(std::ostream& out, const Any& rhs);

        enum class resources: uint32_t {
            total_threads=0,
            total_memory=1,
            hostname=2,
            size=3,
        };

        const char* resource_to_string(resources r) noexcept;
        resources string_to_resource(const char* s, size_t n) noexcept;

        class Bindings {
        public:
            using value_type = uint64_t;
        private:
            // total-threads should be at least 1
            std::array<value_type,size_t(resources::size)> _data{uint64_t{1}};
        public:
            inline value_type get(resources r) const noexcept {
                return this->_data[static_cast<size_t>(r)];
            }
            inline value_type operator[](resources r) const noexcept {
                return this->_data[static_cast<size_t>(r)];
            }
            inline value_type& operator[](resources r) noexcept {
                return this->_data[static_cast<size_t>(r)];
            }
            inline value_type operator[](size_t i) const noexcept { return this->_data[i]; }
            inline value_type& operator[](size_t i) noexcept { return this->_data[i]; }
            inline void set(resources r, value_type value) noexcept {
                this->_data[static_cast<size_t>(r)] = value;
            }
            inline void clear() noexcept { this->_data.fill(value_type{}); }
            static constexpr inline size_t size() noexcept { return size_t(resources::size); }
            inline Bindings& operator+=(const Bindings& y) noexcept {
                const auto n = size();
                for (size_t i=0; i<n; ++i) { this->_data[i] += y._data[i]; }
                return *this;
            }
            inline Bindings& operator-=(const Bindings& y) noexcept {
                const auto n = size();
                for (size_t i=0; i<n; ++i) { this->_data[i] -= y._data[i]; }
                return *this;
            }
            void write(sys::byte_buffer& out) const;
            void read(sys::byte_buffer& in);
            Bindings() = default;
            virtual ~Bindings() = default;
            Bindings(const Bindings&) = default;
            Bindings& operator=(const Bindings&) = default;
            Bindings(Bindings&&) = default;
            Bindings& operator=(Bindings&&) = default;
        };

        inline bool operator==(const Bindings& a, const Bindings& b) {
            const auto n = Bindings::size();
            for (size_t i=0; i<n; ++i) {
                if (a[i] != b[i]) { return false; }
            }
            return true;
        }

        inline bool operator!=(const Bindings& a, const Bindings& b) {
            const auto n = Bindings::size();
            for (size_t i=0; i<n; ++i) {
                if (a[i] == b[i]) { return false; }
            }
            return true;
        }

        inline sys::byte_buffer&
        operator<<(sys::byte_buffer& out, const Bindings& rhs) {
            rhs.write(out); return out;
        }

        inline sys::byte_buffer&
        operator>>(sys::byte_buffer& in, Bindings& rhs) {
            rhs.read(in); return in;
        }

        class Expression {
        public:
            Expression() = default;
            virtual ~Expression() = default;
            Expression(const Expression&) = delete;
            Expression& operator=(const Expression&) = delete;
            Expression(Expression&&) = delete;
            Expression& operator=(Expression&&) = delete;
            virtual Any evaluate(const Bindings& context) const noexcept = 0;
            virtual void write(sys::byte_buffer& out) const = 0;
            virtual void read(sys::byte_buffer& in) = 0;
            virtual void write(std::ostream& out) const = 0;
        };

        inline std::ostream& operator<<(std::ostream& out, const Expression& rhs) {
            rhs.write(out); return out;
        }

        using expression_ptr = std::unique_ptr<Expression>;

        class Symbol: public Expression {
        private:
            resources _name{};
        public:
            inline explicit Symbol(resources name) noexcept: _name(name) {}
            Any evaluate(const Bindings& context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            void write(std::ostream& out) const override;
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
            Any evaluate(const Bindings& context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            void write(std::ostream& out) const override;
            Constant() = default;
            ~Constant() = default;
            Constant(const Constant&) = delete;
            Constant& operator=(const Constant&) = delete;
            Constant(Constant&&) = delete;
            Constant& operator=(Constant&&) = delete;
        };

        #define SBN_RESOURCES_UNARY_OPERATION(NAME) \
            class NAME: public Expression { \
            private: \
                expression_ptr _arg; \
            public: \
                inline explicit NAME(expression_ptr&& arg) noexcept: _arg(std::move(arg)) {} \
                Any evaluate(const Bindings& context) const noexcept override; \
                void write(sys::byte_buffer& out) const override; \
                void read(sys::byte_buffer& in) override; \
                void write(std::ostream& out) const override; \
                NAME() = default; \
                ~NAME() = default; \
                NAME(const NAME&) = delete; \
                NAME& operator=(const NAME&) = delete; \
                NAME(NAME&&) = delete; \
                NAME& operator=(NAME&&) = delete; \
            };

        SBN_RESOURCES_UNARY_OPERATION(Not);
        SBN_RESOURCES_UNARY_OPERATION(Negate);

        #undef SBN_RESOURCES_UNARY_OPERATION

        #define SBN_RESOURCES_BINARY_OPERATION(NAME) \
            class NAME: public Expression { \
            private: \
                expression_ptr _a, _b; \
            public: \
                inline explicit NAME(expression_ptr&& a, expression_ptr&& b) noexcept: \
                _a(std::move(a)), _b(std::move(b)) {} \
                Any evaluate(const Bindings& context) const noexcept override; \
                void write(sys::byte_buffer& out) const override; \
                void read(sys::byte_buffer& in) override; \
                void write(std::ostream& out) const override; \
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
        SBN_RESOURCES_BINARY_OPERATION(Add);
        SBN_RESOURCES_BINARY_OPERATION(Subtract);
        SBN_RESOURCES_BINARY_OPERATION(Multiply);
        SBN_RESOURCES_BINARY_OPERATION(Quotient);
        SBN_RESOURCES_BINARY_OPERATION(Remainder);

        #undef SBN_RESOURCES_BINARY_OPERATION

        expression_ptr read(sys::byte_buffer& in);
        expression_ptr read(const char* begin, const char* end, int max_depth);
        expression_ptr read(std::istream& in, int max_depth);

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
            Add=12,
            Subtract=13,
            Multiply=14,
            Quotient=15,
            Remainder=16,
            Negate=17,
        };

        expression_ptr make_expression(Expressions type);

        inline expression_ptr operator!(expression_ptr&& a) {
            return expression_ptr(new Not(std::move(a)));
        }

        inline expression_ptr operator!(resources r) {
            return !expression_ptr(new Symbol(r));
        }

        inline expression_ptr operator-(expression_ptr&& a) {
            return expression_ptr(new Negate(std::move(a)));
        }

        inline expression_ptr operator-(resources r) {
            return -expression_ptr(new Symbol(r));
        }

        #define SBN_RESOURCES_BINARY_OPERATOR(OP, NAME) \
            inline expression_ptr \
            operator OP(resources a, expression_ptr&& b) { \
                return expression_ptr(new NAME(expression_ptr(new Symbol(a)), std::move(b))); \
            } \
            inline expression_ptr \
            operator OP(resources a, const Any& b) { \
                return expression_ptr(new NAME(expression_ptr(new Symbol(a)), \
                                               expression_ptr(new Constant(b)))); \
            } \
            inline expression_ptr \
            operator OP(expression_ptr&& a, resources b) { \
                return expression_ptr(new NAME(std::move(a), expression_ptr(new Symbol(b)))); \
            } \
            inline expression_ptr \
            operator OP(const Any& a, resources b) { \
                return expression_ptr(new NAME(expression_ptr(new Constant(a)), \
                                               expression_ptr(new Symbol(b)))); \
            }

        SBN_RESOURCES_BINARY_OPERATOR(==, Equal);
        SBN_RESOURCES_BINARY_OPERATOR(<, Less_than);
        SBN_RESOURCES_BINARY_OPERATOR(<=, Less_or_equal);
        SBN_RESOURCES_BINARY_OPERATOR(>, Greater_than);
        SBN_RESOURCES_BINARY_OPERATOR(>=, Greater_or_equal);
        SBN_RESOURCES_BINARY_OPERATOR(&&, And);
        SBN_RESOURCES_BINARY_OPERATOR(||, Or);
        SBN_RESOURCES_BINARY_OPERATOR(^, Xor);
        SBN_RESOURCES_BINARY_OPERATOR(+, Add);
        SBN_RESOURCES_BINARY_OPERATOR(-, Subtract);
        SBN_RESOURCES_BINARY_OPERATOR(*, Multiply);
        SBN_RESOURCES_BINARY_OPERATOR(/, Quotient);
        SBN_RESOURCES_BINARY_OPERATOR(%, Remainder);

        inline expression_ptr
        operator!=(resources a, expression_ptr&& b) {
            return !operator==(a, std::move(b));
        }

        inline expression_ptr
        operator!=(resources a, const Any& b) {
            return !operator==(a, b);
        }

        inline expression_ptr
        operator!=(expression_ptr&& a, resources b) {
            return !operator==(std::move(a), b);
        }

        inline expression_ptr
        operator!=(const Any& a, resources b) {
            return !operator==(a, b);
        }

        #undef SBN_RESOURCES_BINARY_OPERATOR

    }

    using resource_array = resources::Bindings;

}

#endif // vim:filetype=cpp
