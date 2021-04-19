#ifndef SUBORDINATION_CORE_RESOURCES_HH
#define SUBORDINATION_CORE_RESOURCES_HH

#include <array>
#include <memory>
#include <ostream>
#include <type_traits>
#include <unordered_map>

#include <unistdx/base/byte_buffer>

#include <subordination/core/types.hh>

namespace sbn {

    /// \brief Mini-language for selecting nodes with appropriate resources
    /// (memory, cores, graphical accelerators etc.)
    namespace resources {

        class Any {
        public:
            enum class Type: uint8_t {Boolean=0, U64=4, String=5};
        private:
            union {
                bool _b;
                uint64_t _u64;
                char* _string;
            };
            Type _type{};
        public:
            inline Any(bool value) noexcept: _b{value}, _type{Type::Boolean} {}
            inline Any(uint8_t value) noexcept: _u64{value}, _type{Type::U64} {}
            inline Any(uint16_t value) noexcept: _u64{value}, _type{Type::U64} {}
            inline Any(uint32_t value) noexcept: _u64{value}, _type{Type::U64} {}
            inline Any(uint64_t value) noexcept: _u64{value}, _type{Type::U64} {}
            Any(const char* s, size_t n);
            inline Any(const char* s): Any{s, s ? std::char_traits<char>::length(s) : 0} {}
            inline Any(const std::string& s): Any{s.data(), s.size()} {}

            inline Any& operator=(const std::string& s) { *this = Any(s); return *this; }
            inline Any& operator=(const char* s) { *this = Any(s); return *this; }
            inline Any& operator=(uint8_t n) noexcept { *this = Any(n); return *this; }
            inline Any& operator=(uint16_t n) noexcept { *this = Any(n); return *this; }
            inline Any& operator=(uint32_t n) noexcept { *this = Any(n); return *this; }
            inline Any& operator=(uint64_t n) noexcept { *this = Any(n); return *this; }
            inline Any& operator=(bool b) noexcept { *this = Any(b); return *this; }

            inline Type type() const noexcept { return this->_type; }

            inline bool boolean() const noexcept {
                if (this->_type != Type::Boolean) { return false; }
                return this->_b;
            }

            inline uint64_t unsigned_integer() const noexcept {
                if (this->_type != Type::U64) { return 0; }
                return this->_u64;
            }

            bool operator==(const Any& rhs) const noexcept;

            inline bool operator!=(const Any& rhs) const noexcept {
                return !this->operator==(rhs);
            }

            void write(sys::byte_buffer& out) const;
            void write(std::ostream& out) const;
            void read(sys::byte_buffer& in);

            void swap(Any& rhs) noexcept;
            Any() = default;
            ~Any() noexcept;
            Any(const Any&);
            inline Any& operator=(const Any& rhs) { Any tmp(rhs); swap(tmp); return *this; }
            inline Any(Any&& rhs) noexcept { swap(rhs); }
            inline Any& operator=(Any&& rhs) noexcept { Any tmp(std::move(rhs)); swap(tmp); return *this; }

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
            using value_type = Any;
        private:
            // total-threads should be at least 1
            std::array<value_type,size_t(resources::size)> _data{uint64_t{1}};
            std::unordered_map<std::string,value_type> _symbols;
        public:
            inline const value_type& operator[](resources r) const noexcept {
                return this->_data[static_cast<size_t>(r)];
            }
            inline value_type& operator[](resources r) noexcept {
                return this->_data[static_cast<size_t>(r)];
            }
            inline value_type operator[](const std::string& s) const noexcept {
                auto result = this->_symbols.find(s);
                if (result == this->_symbols.end()) { return {}; }
                return result->second;
            }
            inline value_type& operator[](const std::string& s) noexcept {
                return this->_symbols[s];
            }
            inline void unset(const std::string& s) { this->_symbols.erase(s); }
            inline value_type operator[](size_t i) const noexcept { return this->_data[i]; }
            inline value_type& operator[](size_t i) noexcept { return this->_data[i]; }
            inline void clear() noexcept {
                // TODO
                //this->_data.fill(value_type{});
                this->_symbols.clear();
            }
            static constexpr inline size_t size() noexcept { return size_t(resources::size); }
            void write(sys::byte_buffer& out) const;
            void read(sys::byte_buffer& in);
            void write(std::ostream& out) const;
            Bindings() = default;
            virtual ~Bindings() = default;
            Bindings(const Bindings&) = default;
            Bindings& operator=(const Bindings&) = default;
            Bindings(Bindings&&) = default;
            Bindings& operator=(Bindings&&) = default;
            friend bool operator==(const Bindings& a, const Bindings& b);
            friend bool operator!=(const Bindings& a, const Bindings& b);
        };

        inline std::ostream& operator<<(std::ostream& out, const Bindings& rhs) {
            rhs.write(out); return out;
        }

        inline bool operator==(const Bindings& a, const Bindings& b) {
            constexpr const auto n = Bindings::size();
            for (size_t i=0; i<n; ++i) {
                if (a[i] != b[i]) { return false; }
            }
            return a._symbols == b._symbols;
        }

        inline bool operator!=(const Bindings& a, const Bindings& b) {
            return !operator==(a, b);
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

        class Name: public Expression {
        private:
            std::string _name{};
        public:
            inline explicit Name(std::string&& name) noexcept: _name(std::move(name)) {}
            inline explicit Name(const std::string& name): _name(name) {}
            inline explicit Name(const char* name): _name(name) {}
            Any evaluate(const Bindings& context) const noexcept override;
            void write(sys::byte_buffer& out) const override;
            void read(sys::byte_buffer& in) override;
            void write(std::ostream& out) const override;
            Name() = default;
            ~Name() = default;
            Name(const Name&) = delete;
            Name& operator=(const Name&) = delete;
            Name(Name&&) = delete;
            Name& operator=(Name&&) = delete;
        };

        bool is_valid_name(const char* first, const char* last) noexcept;

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

        inline expression_ptr read(const char* s, int max_depth) {
            using t = std::char_traits<char>;
            return read(s, s+t::length(s), max_depth);
        }

        inline expression_ptr read(const std::string& s, int max_depth) {
            return read(s.data(), s.data()+s.size(), max_depth);
        }

        enum class Expressions: uint8_t {
            Symbol=1,
            Constant=2,
            Name=3,
            Not=4,
            And=5,
            Or=6,
            Xor=7,
            Less_than=8,
            Less_or_equal=9,
            Equal=10,
            Greater_than=11,
            Greater_or_equal=12,
            Add=13,
            Subtract=14,
            Multiply=15,
            Quotient=16,
            Remainder=17,
            Negate=18,
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
            } \
            inline expression_ptr \
            operator OP(expression_ptr&& a, expression_ptr&& b) { \
                return expression_ptr(new NAME(std::move(a), std::move(b))); \
            } \
            inline expression_ptr \
            operator OP(expression_ptr&& a, const Any& b) { \
                return expression_ptr(new NAME(std::move(a), \
                                               expression_ptr(new Constant(b)))); \
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
        operator!=(expression_ptr&& a, const Any& b) {
            return !operator==(std::move(a), b);
        }

        inline expression_ptr
        operator!=(expression_ptr&& a, expression_ptr&& b) {
            return !operator==(std::move(a), std::move(b));
        }

        inline expression_ptr
        operator!=(const Any& a, resources b) {
            return !operator==(a, b);
        }

        #undef SBN_RESOURCES_BINARY_OPERATOR

    }

}

#endif // vim:filetype=cpp
