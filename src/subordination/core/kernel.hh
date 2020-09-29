#ifndef SUBORDINATION_CORE_KERNEL_HH
#define SUBORDINATION_CORE_KERNEL_HH

#include <unistdx/base/log_message>
#include <unistdx/net/socket_address>

#include <subordination/core/application.hh>
#include <subordination/core/kernel_base.hh>
#include <subordination/core/kernel_type.hh>
#include <subordination/core/types.hh>

namespace sbn {

    template <class T> using pointer = std::unique_ptr<T>;

    template <class B, class A> inline pointer<B>
    pointer_dynamic_cast(pointer<A>&& ptr) {
        return pointer<B>(dynamic_cast<B*>(ptr.release()));
    }

    template <class T, class ... Args> inline pointer<T>
    make_pointer(Args&& ... args) {
        return pointer<T>(new T(std::forward<Args>(args)...));
    }

    enum class kernel_field: sys::u8 {
        source = 1<<0,
        destination = 1<<1,
        source_application = 1<<2,
        target_application = 1<<3,
    };

    UNISTDX_FLAGS(kernel_field);

    class kernel: public kernel_base {

    public:
        using id_type = uint64_t;
        template <class T> using pointer = ::sbn::pointer<T>;

    public:
        enum class phases: sys::u8 {
            upstream = 1,
            downstream = 2,
            point_to_point = 3,
            broadcast = 4,
        };

    private:
        id_type _id = 0;
        id_type _old_id = 0;
        kernel_field _fields{};
        phases _phase = phases::upstream;
        sys::socket_address _source{};
        sys::socket_address _destination{};
        union {
            application::id_type _source_application_id = this_application::id();
            // TODO application registry
            application* _source_application;
        };
        union {
            application::id_type _target_application_id = this_application::id();
            application* _target_application;
        };
        union {
            kernel* _parent = nullptr;
            id_type _parent_id;
        };
        union {
            kernel* _principal = nullptr;
            id_type _principal_id;
        };
        std::string _path;

    protected:
        // node-local type id
        kernel_type::id_type _type_id = 0;
        kernel_ptr* _this_ptr{};
        // A pipeline that received the kernel. Node-local variable.
        pipeline* _source_pipeline{};

    private:
        bool _routed = false;

    public:

        kernel() = default;
        virtual ~kernel();
        kernel(const kernel&) = delete;
        kernel& operator=(const kernel&) = delete;
        kernel(kernel&&) = delete;
        kernel& operator=(kernel&&) = delete;

        inline id_type id() const noexcept { return this->_id; }
        inline void id(id_type rhs) noexcept { this->_id = rhs; }
        inline bool has_id() const noexcept { return this->_id != 0; }
        inline id_type old_id() const noexcept { return this->_old_id; }
        inline void old_id(id_type rhs) noexcept { this->_old_id = rhs; }
        inline bool routed() const noexcept { return this->_routed; }
        inline void routed(bool rhs) noexcept { this->_routed = rhs; }
        inline kernel_type::id_type type_id() const noexcept { return this->_type_id; }
        inline void type_id(kernel_type::id_type rhs) noexcept { this->_type_id = rhs; }
        inline phases phase() const noexcept { return this->_phase; }
        inline void phase(phases rhs) noexcept { this->_phase = rhs; }
        inline const std::string& path() const noexcept { return this->_path; }
        inline void path(const std::string& rhs) { this->_path = rhs; }
        inline void path(std::string&& rhs) { this->_path = std::move(rhs); }
        inline pipeline* source_pipeline() const noexcept { return this->_source_pipeline; }
        inline void source_pipeline(pipeline* rhs) noexcept { this->_source_pipeline = rhs; }

        inline bool
        operator==(const kernel& rhs) const noexcept {
            return this == &rhs || (this->id() == rhs.id() && this->has_id() && rhs.has_id());
        }

        inline bool
        operator!=(const kernel& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        inline uint64_t
        unique_id() const noexcept {
            return this->has_id() ? this->id() : uint64_t(this);
        }

        inline kernel_field fields() const noexcept { return this->_fields; }

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

        inline application::id_type
        source_application_id() const noexcept {
            return bool(fields() & kernel_field::source_application)
                ? this->_source_application->id() : this->_source_application_id;
        }

        inline void
        source_application_id(application::id_type rhs) noexcept {
            if (bool(fields() & kernel_field::source_application)) {
                delete this->_source_application;
                this->_fields &= ~kernel_field::source_application;
            }
            this->_source_application_id = rhs;
        }

        inline application::id_type
        target_application_id() const noexcept {
            return bool(fields() & kernel_field::target_application)
                ? this->_target_application->id() : this->_target_application_id;
        }

        inline void
        target_application_id(application::id_type rhs) noexcept {
            if (bool(fields() & kernel_field::target_application)) {
                delete this->_target_application;
                this->_fields &= ~kernel_field::target_application;
            }
            this->_target_application_id = rhs;
        }

        inline bool is_foreign() const noexcept { return !this->is_native(); }

        virtual inline bool
        is_native() const noexcept {
            return true;
            //const auto id = this_application::id();
            //return source_application_id() == id || target_application_id() == id;
        }

        inline const application*
        source_application() const {
            if (!bool(fields() & kernel_field::source_application)) {
                return nullptr;
            }
            return this->_source_application;
        }

        inline application*
        source_application() {
            if (!bool(fields() & kernel_field::source_application)) {
                return nullptr;
            }
            return this->_source_application;
        }

        inline void
        source_application(application* rhs) noexcept {
            if (bool(fields() & kernel_field::source_application)) {
                delete this->_source_application;
            }
            this->_source_application = rhs;
            if (this->_source_application) {
                this->_fields |= kernel_field::source_application;
            } else {
                this->_fields &= ~kernel_field::source_application;
            }
        }

        inline const application*
        target_application() const {
            if (!bool(fields() & kernel_field::target_application)) {
                return nullptr;
            }
            return this->_target_application;
        }

        inline application*
        target_application() {
            if (!bool(fields() & kernel_field::target_application)) {
                return nullptr;
            }
            return this->_target_application;
        }

        inline void
        target_application(application* rhs) noexcept {
            if (bool(fields() & kernel_field::target_application)) {
                delete this->_target_application;
            }
            this->_target_application = rhs;
            if (this->_target_application) {
                this->_fields |= kernel_field::target_application;
            } else {
                this->_fields &= ~kernel_field::target_application;
            }
        }

        void write_header(kernel_buffer& out) const;
        void read_header(kernel_buffer& in);
        void swap_header(kernel* k);

        inline const kernel*
        principal() const noexcept {
            if (bool(flags() & kernel_flag::principal_is_id)) { return nullptr; }
            return this->_principal;
        }

        inline kernel* principal() noexcept {
            if (bool(flags() & kernel_flag::principal_is_id)) { return nullptr; }
            return this->_principal;
        }

        inline void
        principal(kernel* rhs) noexcept {
            this->_principal = rhs;
            this->unsetf(kernel_flag::principal_is_id);
        }

        inline id_type
        principal_id() const noexcept {
            if (bool(flags() & kernel_flag::principal_is_id)) {
                return this->_principal_id;
            }
            if (this->_principal) { return this->_principal->id(); }
            return 0;
        }

        inline void
        principal_id(id_type id) noexcept {
            this->_principal_id = id;
            this->_flags |= kernel_flag::principal_is_id;
        }

        inline const kernel*
        parent() const noexcept {
            if (bool(flags() & kernel_flag::parent_is_id)) { return nullptr; }
            return this->_parent;
        }

        inline kernel*
        parent() noexcept {
            if (bool(flags() & kernel_flag::parent_is_id)) { return nullptr; }
            return this->_parent;
        }

        inline bool has_parent() const noexcept {
            if (bool(flags() & kernel_flag::parent_is_id)) { return this->_parent_id != 0; }
            return this->_parent;
        }

        inline void
        parent(kernel* rhs) {
            this->_parent = rhs;
            this->_flags &= ~kernel_flag::parent_is_id;
        }

        inline id_type
        parent_id() const noexcept {
            if (bool(flags() & kernel_flag::parent_is_id)) {
                return this->_parent_id;
            }
            if (this->_parent) { return this->_parent->id(); }
            return 0;
        }

        inline void
        parent_id(id_type rhs) noexcept {
            if (!bool(flags() & kernel_flag::parent_is_id)) {
                //delete this->_parent;
                this->_flags |= kernel_flag::parent_is_id;
            }
            this->_parent_id = rhs;
        }

        inline size_t
        hash() const noexcept{
            size_t ret;
            if (bool(flags() & kernel_flag::principal_is_id)) {
                ret = this->_principal_id;
            } else if (!bool(flags() & kernel_flag::principal_is_id) &&
                       this->_principal && this->_principal->has_id()) {
                ret = principal()->id();
            } else {
                ret = size_t(this->_principal) / alignof(size_t);
            }
            return ret;
        }

        virtual void read(kernel_buffer& in);
        virtual void write(kernel_buffer& out) const;

        /// \brief Performs the task or launches subordinate kernels to do so.
        virtual void act();

        /// \brief Collects the output from the task from subordinate kernel \p child.
        virtual void react(kernel_ptr&& child);

        /// \brief Undo the side effects caused by failed execution of the kernel.
        virtual void rollback();

        /// \brief Garbage-collect.
        virtual void mark_as_deleted(kernel_sack& result);

        friend std::ostream&
        operator<<(std::ostream& out, const kernel& rhs);

    public:

        /// New API

        inline void
        return_to_parent(exit_code ret) noexcept {
            if (bool(flags() & kernel_flag::parent_is_id)) {
                principal_id(this->_parent_id);
            } else {
                principal(this->_parent);
            }
            return_code(ret);
            destination(source());
            phase(phases::downstream);
            // swap fields
            using f = kernel_field;
            auto source_bit = fields() & f::source_application;
            auto target_bit = fields() & f::target_application;
            this->_fields &= ~(f::source_application | f::target_application);
            if (bool(source_bit)) { this->_fields |= f::target_application; }
            if (bool(target_bit)) { this->_fields |= f::source_application; }
            std::swap(this->_source_application_id, this->_target_application_id);
        }

        inline void
        return_to_parent() noexcept {
            return_to_parent(return_code() == exit_code::undefined
                             ? exit_code::success : return_code());
        }

        inline void point_to_point(kernel* k) {
            principal(k);
            phase(phases::point_to_point);
        }

        inline void point_to_point(kernel::id_type id) {
            principal_id(id);
            phase(phases::point_to_point);
        }

        inline void this_ptr(kernel_ptr* rhs) noexcept { this->_this_ptr = rhs; }

    protected:

        inline kernel_ptr& this_ptr() noexcept { return *this->_this_ptr; }
        inline const kernel_ptr& this_ptr() const noexcept { return *this->_this_ptr; }

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
