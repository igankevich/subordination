#include <cstdlib>
#include <sstream>

#include <subordination/core/resources.hh>

namespace {

    inline void trim(const char** first_inout, const char** last_inout) {
        auto first = *first_inout, last = *last_inout;
        while (first != last && std::isspace(*first)) { ++first; }
        *first_inout = first;
        while (first != last && std::isspace(*(last-1))) { --last; }
        *last_inout = last;
    }

    constexpr const int max_depth = 10;

    inline sbn::resources::expression_ptr
    read_expression(const char* first, const char* last, int depth) {
        trim(&first, &last);
        using namespace sbn::resources;
        expression_ptr result;
        auto name_last = first;
        while (name_last != last && !std::isspace(*name_last)) { ++name_last; }
        std::string name(first, name_last);
        while (name_last != last && std::isspace(*name_last)) { ++name_last; }
        if (name == "not") {
            // parse one arguments
            auto arg = read(name_last, last, depth-1);
            result = expression_ptr(new Not(std::move(arg)));
        } else if (name == "negate") {
            // parse one arguments
            auto arg = read(name_last, last, depth-1);
            result = expression_ptr(new Negate(std::move(arg)));
        } else {
            // parse two arguments
            int bracket = 0;
            auto ptr = name_last;
            while (ptr != last && (bracket != 0 || !std::isspace(*ptr))) {
                if (*ptr == '(') { ++bracket; }
                else if (*ptr == ')') { --bracket; }
                if (bracket < 0) { return nullptr; }
                ++ptr;
            }
            if (ptr == last) { return nullptr; }
            auto arg0 = read(name_last, ptr, depth-1);
            auto arg1 = read(ptr, last, depth-1);
            if (name == "and") {
                result = expression_ptr(new And(std::move(arg0), std::move(arg1)));
            } else if (name == "or") {
                result = expression_ptr(new Or(std::move(arg0), std::move(arg1)));
            } else if (name == "xor") {
                result = expression_ptr(new Xor(std::move(arg0), std::move(arg1)));
            } else if (name == "=") {
                result = expression_ptr(new Equal(std::move(arg0), std::move(arg1)));
            } else if (name == "<") {
                result = expression_ptr(new Less_than(std::move(arg0), std::move(arg1)));
            } else if (name == "<=") {
                result = expression_ptr(new Less_or_equal(std::move(arg0), std::move(arg1)));
            } else if (name == ">") {
                result = expression_ptr(new Greater_than(std::move(arg0), std::move(arg1)));
            } else if (name == ">=") {
                result = expression_ptr(new Greater_or_equal(std::move(arg0), std::move(arg1)));
            } else if (name == "+") {
                result = expression_ptr(new Add(std::move(arg0), std::move(arg1)));
            } else if (name == "-") {
                result = expression_ptr(new Subtract(std::move(arg0), std::move(arg1)));
            } else if (name == "*") {
                result = expression_ptr(new Multiply(std::move(arg0), std::move(arg1)));
            } else if (name == "quotient") {
                result = expression_ptr(new Quotient(std::move(arg0), std::move(arg1)));
            } else if (name == "remainder") {
                result = expression_ptr(new Remainder(std::move(arg0), std::move(arg1)));
            }
        }
        return result;
    }

    inline void write_string(std::ostream& out, const char* s) {
        out.put('"');
        while (*s) {
            if (*s == '"') { out.put('\\').put('"'); }
            else { out.put(*s); }
            ++s;
        }
        out.put('"');
    }

    inline std::string read_string(const char* s, size_t n) {
        std::string buf;
        buf.reserve(n);
        auto first = s, last = s+n;
        bool escape = false;
        while (first != last) {
            if (escape) {
                buf += *first;
                escape = false;
            } else if (*first == '\\') {
                escape = true;
            } else {
                buf += *first;
            }
            ++first;
        }
        return buf;
    }

    constexpr const auto bad_resource =
        sbn::resources::resources(
            std::numeric_limits<std::underlying_type<sbn::resources::resources>::type>::max());

}

sbn::resources::Any::Any(const char* s, size_t n): _type{Type::String} {
    using t = std::char_traits<char>;
    this->_string = new char[n+1];
    t::copy(this->_string, s, n);
    this->_string[n] = 0;
}

sbn::resources::Any::~Any() noexcept {
    if (type() == Type::String) { delete this->_string; }
}

void sbn::resources::Any::swap(Any& rhs) noexcept {
    using std::swap;
    swap(this->_type, rhs._type);
    switch (this->_type) {
        case Any::Type::Boolean: swap(this->_b, rhs._b); break;
        case Any::Type::U8: swap(this->_u8, rhs._u8); break;
        case Any::Type::U16: swap(this->_u16, rhs._u16); break;
        case Any::Type::U32: swap(this->_u32, rhs._u32); break;
        case Any::Type::U64: swap(this->_u64, rhs._u64); break;
        case Any::Type::String: swap(this->_string, rhs._string); break;
        default: break;
    }
}

sbn::resources::Any::Any(const Any& rhs): _type{rhs._type} {
    switch (this->_type) {
        case Any::Type::Boolean: this->_b = rhs._b; break;
        case Any::Type::U8: this->_u8 = rhs._u8; break;
        case Any::Type::U16: this->_u16 = rhs._u16; break;
        case Any::Type::U32: this->_u32 = rhs._u32; break;
        case Any::Type::U64: this->_u64 = rhs._u64; break;
        case Any::Type::String: {
            using t = std::char_traits<char>;
            auto n = t::length(rhs._string);
            this->_string = new char[n+1];
            t::copy(this->_string, rhs._string, n);
            this->_string[n] = 0;
            break;
        }
        default: break;
    }
}

auto sbn::resources::Any::operator=(const Any& rhs) -> Any& {
    Any tmp(rhs);
    swap(tmp);
    return *this;
}

auto sbn::resources::Symbol::evaluate(Bindings& b) const noexcept -> Any {
    return b.get(this->_name);
}
auto sbn::resources::Constant::evaluate(Bindings& b) const noexcept -> Any {
    return this->_value;
}
auto sbn::resources::Not::evaluate(Bindings& b) const noexcept -> Any {
    return !this->_arg->evaluate(b).boolean();
}
auto sbn::resources::Negate::evaluate(Bindings& b) const noexcept -> Any {
    return -this->_arg->evaluate(b).unsigned_integer();
}
auto sbn::resources::And::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).boolean() && this->_b->evaluate(b).boolean();
}
auto sbn::resources::Or::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).boolean() || this->_b->evaluate(b).boolean();
}
auto sbn::resources::Xor::evaluate(Bindings& b) const noexcept -> Any {
    return bool(this->_a->evaluate(b).boolean() ^ this->_b->evaluate(b).boolean());
}
auto sbn::resources::Less_than::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() < this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Less_or_equal::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() <= this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Equal::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() == this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Greater_than::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() > this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Greater_or_equal::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() >= this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Add::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() + this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Subtract::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() - this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Multiply::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() * this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Quotient::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() / this->_b->evaluate(b).unsigned_integer();
}
auto sbn::resources::Remainder::evaluate(Bindings& b) const noexcept -> Any {
    return this->_a->evaluate(b).unsigned_integer() % this->_b->evaluate(b).unsigned_integer();
}

void sbn::resources::Any::write(sys::byte_buffer& out) const {
    using t = std::char_traits<char>;
    out.write(this->_type);
    switch (this->_type) {
        case Any::Type::Boolean: out.write(this->_b); break;
        case Any::Type::U8: out.write(this->_u8); break;
        case Any::Type::U16: out.write(this->_u16); break;
        case Any::Type::U32: out.write(this->_u32); break;
        case Any::Type::U64: out.write(this->_u64); break;
        case Any::Type::String: {
            const uint32_t n = t::length(this->_string);
            out.write(n);
            out.write(this->_string, n);
            break;
        }
        default: break;
    }
}

void sbn::resources::Any::read(sys::byte_buffer& in) {
    in.read(this->_type);
    switch (this->_type) {
        case Any::Type::Boolean: in.read(this->_b); break;
        case Any::Type::U8: in.read(this->_u8); break;
        case Any::Type::U16: in.read(this->_u16); break;
        case Any::Type::U32: in.read(this->_u32); break;
        case Any::Type::U64: in.read(this->_u64); break;
        case Any::Type::String: {
            uint32_t n = 0;
            in.read(n);
            in.read(this->_string, n);
            break;
        }
        default: break;
    }
}

void sbn::resources::Symbol::write(sys::byte_buffer& out) const {
    out.write(Expressions::Symbol);
    out.write(this->_name);
}
void sbn::resources::Symbol::read(sys::byte_buffer& in) { in.read(this->_name); }
void sbn::resources::Constant::write(sys::byte_buffer& out) const {
    out.write(Expressions::Constant);
    this->_value.write(out);
}
void sbn::resources::Constant::read(sys::byte_buffer& in) { this->_value.read(in); }

#define SBN_RESOURCES_UNARY_OPERATION_IO(NAME, HUMAN_NAME) \
    void sbn::resources::NAME::write(sys::byte_buffer& out) const { this->_arg->write(out); } \
    void sbn::resources::NAME::read(sys::byte_buffer& in) { \
        this->_arg = ::sbn::resources::read(in); \
    } \
    void sbn::resources::NAME::write(std::ostream& out) const { \
        out << "(" HUMAN_NAME " " << *this->_arg << ')'; \
    }

SBN_RESOURCES_UNARY_OPERATION_IO(Not, "not");
SBN_RESOURCES_UNARY_OPERATION_IO(Negate, "negate");

#define SBN_RESOURCES_BINARY_OPERATION_IO(NAME, HUMAN_NAME) \
    void sbn::resources::NAME::write(sys::byte_buffer& out) const { \
        out.write(Expressions::NAME); \
        this->_a->write(out); \
        this->_b->write(out); \
    } \
    void sbn::resources::NAME::read(sys::byte_buffer& in) { \
        this->_a = ::sbn::resources::read(in); \
        this->_b = ::sbn::resources::read(in); \
    } \
    void sbn::resources::NAME::write(std::ostream& out) const { \
        out << "(" HUMAN_NAME " " << *this->_a << ' ' << *this->_b << ')'; \
    }

SBN_RESOURCES_BINARY_OPERATION_IO(And, "and");
SBN_RESOURCES_BINARY_OPERATION_IO(Or, "or");
SBN_RESOURCES_BINARY_OPERATION_IO(Xor, "xor");
SBN_RESOURCES_BINARY_OPERATION_IO(Less_than, "<");
SBN_RESOURCES_BINARY_OPERATION_IO(Less_or_equal, "<=");
SBN_RESOURCES_BINARY_OPERATION_IO(Equal, "=");
SBN_RESOURCES_BINARY_OPERATION_IO(Greater_than, ">");
SBN_RESOURCES_BINARY_OPERATION_IO(Greater_or_equal, ">=");
SBN_RESOURCES_BINARY_OPERATION_IO(Add, "+");
SBN_RESOURCES_BINARY_OPERATION_IO(Subtract, "-");
SBN_RESOURCES_BINARY_OPERATION_IO(Multiply, "*");
SBN_RESOURCES_BINARY_OPERATION_IO(Quotient, "quotient");
SBN_RESOURCES_BINARY_OPERATION_IO(Remainder, "remainder");

auto sbn::resources::make_expression(Expressions type) -> expression_ptr {
    switch (type) {
        case Expressions::Symbol: return expression_ptr(new Symbol);
        case Expressions::Constant: return expression_ptr(new Constant);
        case Expressions::Not: return expression_ptr(new Not);
        case Expressions::And: return expression_ptr(new And);
        case Expressions::Or: return expression_ptr(new Or);
        case Expressions::Xor: return expression_ptr(new Xor);
        case Expressions::Less_than: return expression_ptr(new Less_than);
        case Expressions::Less_or_equal: return expression_ptr(new Less_or_equal);
        case Expressions::Equal: return expression_ptr(new Equal);
        case Expressions::Greater_than: return expression_ptr(new Greater_than);
        case Expressions::Greater_or_equal: return expression_ptr(new Greater_or_equal);
        case Expressions::Add: return expression_ptr(new Add);
        case Expressions::Subtract: return expression_ptr(new Subtract);
        case Expressions::Multiply: return expression_ptr(new Multiply);
        case Expressions::Quotient: return expression_ptr(new Quotient);
        case Expressions::Remainder: return expression_ptr(new Remainder);
        default: throw std::invalid_argument("bad expression type");
    }
}

auto sbn::resources::read(sys::byte_buffer& in) -> expression_ptr {
    Expressions type{};
    in.read(type);
    auto result = make_expression(type);
    result->read(in);
    return result;
}

void sbn::resources::Any::write(std::ostream& out) const {
    switch (this->_type) {
        case Any::Type::Boolean: out << this->_b; break;
        case Any::Type::U8: out << this->_u8; break;
        case Any::Type::U16: out << this->_u16; break;
        case Any::Type::U32: out << this->_u32; break;
        case Any::Type::U64: out << this->_u64; break;
        case Any::Type::String: write_string(out, this->_string); break;
        default: break;
    }
}

std::ostream& sbn::resources::operator<<(std::ostream& out, const Any& rhs) {
    rhs.write(out);
    return out;
}

void sbn::resources::Symbol::write(std::ostream& out) const {
    auto s = resource_to_string(this->_name);
    out << (s ? s : "unknown");
}

void sbn::resources::Constant::write(std::ostream& out) const {
    out << this->_value;
}

const char* sbn::resources::resource_to_string(resources r) noexcept {
    switch (r) {
        case resources::total_threads: return "total-threads";
        case resources::total_memory: return "total-memory";
        case resources::hostname: return "hostname";
        default: return nullptr;
    }
}

auto sbn::resources::string_to_resource(const char* s, size_t n) noexcept -> resources {
    using t = std::char_traits<char>;
    if (t::compare(s, "total-threads", n) == 0) { return resources::total_threads; }
    if (t::compare(s, "total-memory", n) == 0) { return resources::total_memory; }
    if (t::compare(s, "hostname", n) == 0) { return resources::hostname; }
    return bad_resource;
}

auto sbn::resources::read(const char* first, const char* last, int depth) -> expression_ptr {
    if (depth <= 0) { return nullptr; }
    trim(&first, &last);
    if (first == last) { return nullptr; }
    expression_ptr result;
    if (*first == '(' && last[-1] == ')') {
        result = read_expression(first+1, last-1, depth);
    } else if (*first == '"' && last[-1] == '"') {
        ++first, --last;
        result = expression_ptr(new Constant(Any(read_string(first,last-first).data())));
    } else {
        auto r = string_to_resource(first, last-first);
        if (r != bad_resource) {
            result = expression_ptr(new Symbol(r));
        } else {
            uint64_t v = std::stol(std::string(first, last));
            result = expression_ptr(new Constant(v));
        }
    }
    return result;
};

auto sbn::resources::read(std::istream& in, int max_depth) -> expression_ptr {
    std::stringstream tmp;
    tmp << in.rdbuf();
    auto s = tmp.str();
    return read(s.data(), s.data()+s.size(), max_depth);
}

void sbn::resources::Bindings::write(sys::byte_buffer& out) const {
    for (const auto& x : this->_data) { out.write(x); }
}

void sbn::resources::Bindings::read(sys::byte_buffer& in) {
    for (auto& x : this->_data) { in.read(x); }
}
