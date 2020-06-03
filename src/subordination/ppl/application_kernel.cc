#include <subordination/ppl/application.hh>
#include <subordination/ppl/application_kernel.hh>

namespace {

    void
    write_vector(sys::pstream& out, const std::vector<std::string>& rhs) {
        const uint32_t n = rhs.size();
        out << n;
        for (uint32_t i=0; i<n; ++i) {
            out << rhs[i];
        }
    }

    void
    read_vector(sys::pstream& in, std::vector<std::string>& rhs) {
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
sbn::application_kernel
::write(sys::pstream& out) const {
    kernel::write(out);
    write_vector(out, this->_args);
    write_vector(out, this->_env);
    out << this->_workdir << this->_error << this->_application;
}

void
sbn::application_kernel
::read(sys::pstream& in) {
    kernel::read(in);
    read_vector(in, this->_args);
    read_vector(in, this->_env);
    in >> this->_workdir >> this->_error >> this->_application;
}

void sbn::application_kernel::act() {
    // this method is implemented in daemon
    delete this;
}
