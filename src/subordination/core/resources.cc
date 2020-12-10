#include <subordination/core/resources.hh>

auto sbn::resources::Symbol::evaluate(Context* c) const noexcept -> Any {
    return c->get(this->_name);
}
auto sbn::resources::Constant::evaluate(Context* c) const noexcept -> Any {
    return this->_value;
}
auto sbn::resources::Not::evaluate(Context* c) const noexcept -> Any {
    return !cast<bool>(this->_arg->evaluate(c));
}
auto sbn::resources::And::evaluate(Context* c) const noexcept -> Any {
    return cast<bool>(this->_a->evaluate(c)) && cast<bool>(this->_b->evaluate(c));
}
auto sbn::resources::Or::evaluate(Context* c) const noexcept -> Any {
    return cast<bool>(this->_a->evaluate(c)) || cast<bool>(this->_b->evaluate(c));
}
auto sbn::resources::Xor::evaluate(Context* c) const noexcept -> Any {
    return bool(cast<bool>(this->_a->evaluate(c)) ^ cast<bool>(this->_b->evaluate(c)));
}
auto sbn::resources::Less_than::evaluate(Context* c) const noexcept -> Any {
    return cast<uint64_t>(this->_a->evaluate(c)) < cast<uint64_t>(this->_b->evaluate(c));
}
auto sbn::resources::Less_or_equal::evaluate(Context* c) const noexcept -> Any {
    return cast<uint64_t>(this->_a->evaluate(c)) <= cast<uint64_t>(this->_b->evaluate(c));
}
auto sbn::resources::Equal::evaluate(Context* c) const noexcept -> Any {
    return cast<uint64_t>(this->_a->evaluate(c)) == cast<uint64_t>(this->_b->evaluate(c));
}
auto sbn::resources::Greater_than::evaluate(Context* c) const noexcept -> Any {
    return cast<uint64_t>(this->_a->evaluate(c)) > cast<uint64_t>(this->_b->evaluate(c));
}
auto sbn::resources::Greater_or_equal::evaluate(Context* c) const noexcept -> Any {
    return cast<uint64_t>(this->_a->evaluate(c)) >= cast<uint64_t>(this->_b->evaluate(c));
}

void sbn::resources::Any::write(sys::byte_buffer& out) const {
    out.write(this->_type);
    switch (this->_type) {
        case Any::Type::Boolean: out.write(this->_b); break;
        case Any::Type::U8: out.write(this->_u8); break;
        case Any::Type::U16: out.write(this->_u16); break;
        case Any::Type::U32: out.write(this->_u32); break;
        case Any::Type::U64: out.write(this->_u64); break;
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
void sbn::resources::Not::write(sys::byte_buffer& out) const { this->_arg->write(out); }
void sbn::resources::Not::read(sys::byte_buffer& in) {
    this->_arg = ::sbn::resources::read(in);
}

#define SBN_RESOURCES_BINARY_OPERATION_IO(NAME) \
    void sbn::resources::NAME::write(sys::byte_buffer& out) const { \
        out.write(Expressions::NAME); \
        this->_a->write(out); \
        this->_b->write(out); \
    } \
    void sbn::resources::NAME::read(sys::byte_buffer& in) { \
        this->_a = ::sbn::resources::read(in); \
        this->_b = ::sbn::resources::read(in); \
    }

SBN_RESOURCES_BINARY_OPERATION_IO(And);
SBN_RESOURCES_BINARY_OPERATION_IO(Or);
SBN_RESOURCES_BINARY_OPERATION_IO(Xor);
SBN_RESOURCES_BINARY_OPERATION_IO(Less_than);
SBN_RESOURCES_BINARY_OPERATION_IO(Less_or_equal);
SBN_RESOURCES_BINARY_OPERATION_IO(Equal);
SBN_RESOURCES_BINARY_OPERATION_IO(Greater_than);
SBN_RESOURCES_BINARY_OPERATION_IO(Greater_or_equal);

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
