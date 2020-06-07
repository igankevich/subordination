#ifndef SUBORDINATION_CORE_KERNEL_HEADER_HH
#define SUBORDINATION_CORE_KERNEL_HEADER_HH

#include <iosfwd>
#include <memory>

#include <unistdx/net/socket_address>

#include <subordination/core/application.hh>
#include <subordination/core/kernel_header_flag.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class kernel_header {

    private:
        kernel_field _fields{};
        sys::socket_address _source{};
        sys::socket_address _destination{};
        union {
            ::sbn::application::id_type  _application_id = this_application::get_id();
            // TODO application registry
            ::sbn::application* _application;
        };

    public:
        kernel_header() = default;
        kernel_header(const kernel_header&) = delete;
        kernel_header& operator=(const kernel_header&) = delete;

        inline
        ~kernel_header() {
            if (bool(fields() & kernel_field::application)) {
                delete this->_application;
            }
        }

        inline const kernel_field& fields() const noexcept {
            return this->_fields;
        }

        inline const sys::socket_address& source() const noexcept {
            return this->_source;
        }

        inline const sys::socket_address& destination() const noexcept {
            return this->_destination;
        }

        inline void source(const sys::socket_address& rhs) noexcept {
            this->_source = rhs;
        }

        inline void destination(const sys::socket_address& rhs) noexcept {
            this->_destination = rhs;
        }

        inline ::sbn::application::id_type
        application_id() const noexcept {
            return bool(fields() & kernel_field::application)
                ? this->_application->id() : this->_application_id;
        }

        inline void
        application_id(::sbn::application::id_type rhs) noexcept {
            if (bool(fields() & kernel_field::application)) {
                delete this->_application;
                this->_fields &= ~kernel_field::application;
            }
            this->_application_id = rhs;
        }

        inline bool is_foreign() const noexcept { return !this->is_native(); }

        inline bool
        is_native() const noexcept {
            return application_id() == this_application::get_id();
        }

        inline const ::sbn::application*
        application() const {
            if (!bool(fields() & kernel_field::application)) {
                return nullptr;
            }
            return this->_application;
        }

        inline void
        application(::sbn::application* rhs) noexcept {
            if (bool(fields() & kernel_field::application)) {
                delete this->_application;
            }
            this->_application = rhs;
            if (this->_application) {
                this->_fields |= kernel_field::application;
            } else {
                this->_fields &= ~kernel_field::application;
            }
        }

        void write_header(kernel_buffer& out) const;
        void read_header(kernel_buffer& in);

        friend std::ostream&
        operator<<(std::ostream& out, const kernel_header& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const kernel_header& rhs);

}

#endif // vim:filetype=cpp
