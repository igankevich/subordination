#include <subordination/core/application.hh>
#include <subordination/core/application_kernel.hh>
#include <subordination/core/kernel_buffer.hh>

namespace {

    void
    write_vector(sbn::kernel_buffer& out, const std::vector<std::string>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (uint32_t i=0; i<n; ++i) {
            out << rhs[i];
        }
    }

    void
    read_vector(sbn::kernel_buffer& in, std::vector<std::string>& rhs) {
        rhs.clear();
        uint32_t n = 0;
        in >> n;
        rhs.resize(n);
        for (uint32_t i=0; i<n; ++i) {
            in >> rhs[i];
        }
    }

}

void
sbn::application_kernel::write(sbn::kernel_buffer& out) const {
    kernel::write(out);
    write_vector(out, this->_args);
    write_vector(out, this->_env);
    out << this->_workdir << this->_error << this->_application;
}

void
sbn::application_kernel::read(sbn::kernel_buffer& in) {
    kernel::read(in);
    read_vector(in, this->_args);
    read_vector(in, this->_env);
    in >> this->_workdir >> this->_error >> this->_application;
}

void sbn::application_kernel::act() {
    // this method is implemented in daemon
    delete this;
}
