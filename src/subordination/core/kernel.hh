#ifndef SUBORDINATION_CORE_KERNEL_HH
#define SUBORDINATION_CORE_KERNEL_HH

#include <unistdx/net/socket_address>

#include <subordination/core/application.hh>
#include <subordination/core/kernel_base.hh>
#include <subordination/core/kernel_header_flag.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class kernel: public kernel_base {

    public:
        using id_type = uint64_t;

    private:
        id_type _id = no_id();
        kernel_field _fields{};
        sys::socket_address _source{};
        sys::socket_address _destination{};
        union {
            ::sbn::application::id_type  _application_id = this_application::get_id();
            // TODO application registry
            ::sbn::application* _application;
        };
        union {
            kernel* _parent = nullptr;
            id_type _parent_id;
        };
        union {
            kernel* _principal = nullptr;
            id_type _principal_id;
        };

    public:

        kernel() = default;
        virtual ~kernel();
        kernel(const kernel&) = delete;
        kernel& operator=(const kernel&) = delete;
        kernel(kernel&&) = delete;
        kernel& operator=(kernel&&) = delete;

        inline id_type id() const noexcept { return this->_id; }
        inline void id(id_type rhs) noexcept { this->_id = rhs; }
        inline bool has_id() const noexcept { return this->_id != no_id(); }
        inline void set_id(id_type rhs) noexcept { this->_id = rhs; }

        inline bool
        operator==(const kernel& rhs) const noexcept {
            return this == &rhs || (this->id() == rhs.id() && this->has_id() && rhs.has_id());
        }

        inline bool
        operator!=(const kernel& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        static constexpr id_type no_id() noexcept { return 0; }

        inline uint64_t
        unique_id() const noexcept {
            return this->has_id() ? this->id() : uint64_t(this);
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
        void swap_header(kernel* k);

        inline const kernel*
        principal() const {
            return this->isset(kernel_flag::principal_is_id)
                ? nullptr : this->_principal;
        }

        inline kernel* principal() noexcept { return this->_principal; }

        inline id_type
        principal_id() const {
            return this->_principal_id;
        }

        inline void
        set_principal_id(id_type id) {
            this->_principal_id = id;
            this->setf(kernel_flag::principal_is_id);
        }

        inline void
        principal(kernel* rhs) {
            this->_principal = rhs;
            this->unsetf(kernel_flag::principal_is_id);
        }

        inline const kernel*
        parent() const {
            return this->_parent;
        }

        inline kernel*
        parent() {
            return this->_parent;
        }

        inline id_type
        parent_id() const {
            return this->_parent_id;
        }

        inline void
        parent(kernel* p) {
            this->_parent = p;
            this->unsetf(kernel_flag::parent_is_id);
        }

        inline size_t
        hash() const {
            const bool b = this->isset(kernel_flag::principal_is_id);
            size_t ret;
            if (b) {
                ret = this->_principal_id;
            } else {
                ret = this->_principal && this->_principal->has_id()
                    ? this->_principal->id()
                    : size_t(this->_principal) / alignof(size_t);
            }
            return ret;
        }

        inline bool
        moves_upstream() const noexcept {
            return this->return_code() == exit_code::undefined &&
                !this->_principal &&
                this->_parent;
        }

        inline bool
        moves_downstream() const noexcept {
            return this->return_code() != exit_code::undefined &&
                this->_principal &&
                this->_parent;
        }

        inline bool
        moves_somewhere() const noexcept {
            return this->return_code() == exit_code::undefined &&
                this->_principal &&
                this->_parent;
        }

        inline bool
        moves_everywhere() const noexcept {
            return !this->_principal && !this->_parent;
        }

        virtual void read(kernel_buffer& in);
        virtual void write(kernel_buffer& out) const;

        /// \brief Performs the task or launches subordinate kernels to do so.
        virtual void act();

        /// \brief Collects the output from the task from subordinate kernel \p child.
        virtual void react(kernel* child);

        virtual void
        error(kernel* rhs);

        friend std::ostream&
        operator<<(std::ostream& out, const kernel& rhs);

    public:

        /// New API

        inline kernel*
        call(kernel* rhs) noexcept {
            rhs->parent(this);
            return rhs;
        }

        inline kernel*
        carry_parent(kernel* rhs) noexcept {
            rhs->parent(this);
            rhs->setf(kernel_flag::carries_parent);
            return rhs;
        }

        inline void
        return_to_parent(exit_code ret = exit_code::success) noexcept {
            return_to(_parent, ret);
            if (source()) {
                this->destination(source());
            }
        }

        inline void
        return_to(kernel* rhs, exit_code ret = exit_code::success) noexcept {
            this->principal(rhs);
            this->return_code(ret);
        }

        inline void
        recurse() noexcept {
            this->principal(this);
        }

        template <class Container> void
        mark_as_deleted(Container& result) noexcept {
            if (this->isset(kernel_flag::deleted)) { return; }
            this->setf(kernel_flag::deleted);
            if (is_native() && this->_parent) { this->_parent->mark_as_deleted(result); }
            result.emplace_back(this);
        }

    };

    std::ostream& operator<<(std::ostream& out, const kernel& rhs);

    inline sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const kernel& rhs) {
        rhs.write(out);
        return out;
    }

    inline sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, kernel& rhs) {
        rhs.read(in);
        return in;
    }

}

#endif // vim:filetype=cpp
