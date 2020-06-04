#include <subordination/core/foreign_kernel.hh>

void
sbn::foreign_kernel
::write(sys::pstream& out) const {
    out << this->_type;
    sbn::kernel::write(out);
    out.write(this->_payload, this->_size);
}

void
sbn::foreign_kernel
::read(sys::pstream& in) {
    sys::packetbuf* buf = in.rdbuf();
    in >> this->_type;
    sbn::kernel::read(in);
    if (this->_payload) {
        this->free();
    }
    char_type* first = buf->ipayload_cur();
    char_type* last = buf->ipayload_end();
    this->_size = last - first;
    this->_payload = new char_type[this->_size];
    in.read(this->_payload, this->_size);
}
