#ifndef SUBORDINATION_GUILE_BASE_HH
#define SUBORDINATION_GUILE_BASE_HH

#include <libguile.h>

#include <string>

#include <subordination/core/kernel_buffer.hh>

namespace sbn {

    namespace guile {

        void object_write(kernel_buffer& buffer, SCM object);
        void object_read(kernel_buffer& buffer, SCM& result);

        auto object_to_string(SCM scm) -> std::string;
        auto string_to_object(const std::string& s) -> SCM;

        template <class T> T*
        allocate(const char* name) {
            return reinterpret_cast<T*>(scm_gc_malloc(sizeof(T), name));
        }

        template <class T, class ... Args> T*
        construct(const char* name, Args ... args) {
            auto* ptr = allocate<T>(name);
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }

        inline bool is_bound(SCM s) { return !SCM_UNBNDP(s); }
        inline bool is_unspecified(SCM s) { return s == SCM_UNSPECIFIED; }
        inline bool symbol_equal(SCM a, SCM b) { return scm_is_true(scm_eq_p(a, b)); }

        struct c_deleter {
            inline void operator()(void* ptr) { std::free(ptr); }
        };
        using c_string = std::unique_ptr<char,c_deleter>;

        inline c_string to_c_string(SCM s) { return c_string(scm_to_utf8_string(s)); }

        class protected_scm {

        private:
            SCM _value = SCM_UNSPECIFIED;

        public:
            inline protected_scm(SCM value) noexcept: _value(value) { protect(); }
            inline ~protected_scm() noexcept { unprotect(); }
            inline protected_scm(protected_scm&& rhs) noexcept:
            _value(rhs._value) { rhs._value = SCM_UNSPECIFIED; }
            inline protected_scm& operator=(const protected_scm& rhs) noexcept {
                unprotect();
                this->_value = rhs._value;
                return *this;
            }
            inline protected_scm& operator=(protected_scm&& rhs) noexcept {
                swap(rhs);
                return *this;
            }
            inline protected_scm& operator=(SCM rhs) noexcept {
                unprotect();
                this->_value = rhs;
                protect();
                return *this;
            }
            protected_scm() = default;
            protected_scm(const protected_scm&) = default;

            inline void swap(protected_scm& rhs) noexcept {
                std::swap(this->_value, rhs._value);
            }

            inline operator SCM() noexcept { return this->_value; }
            inline operator SCM() const noexcept { return this->_value; }
            inline SCM& get() noexcept { return this->_value; }
            inline const SCM& get() const noexcept { return this->_value; }

        private:
            inline void protect() noexcept {
                if (this->_value != SCM_UNSPECIFIED && this->_value != SCM_UNDEFINED) {
                    scm_gc_protect_object(this->_value);
                }
            }
            inline void unprotect() noexcept {
                if (this->_value != SCM_UNSPECIFIED && this->_value != SCM_UNDEFINED) {
                    scm_gc_unprotect_object(this->_value);
                }
            }

            friend inline void
            object_write(kernel_buffer& buffer, const protected_scm& object) {
                object_write(buffer, object.get());
            }

            friend inline void
            object_read(kernel_buffer& buffer, protected_scm& result) {
                SCM tmp = SCM_UNDEFINED;
                object_read(buffer, tmp);
                result = tmp;
            }

        };

    }

}

#endif // vim:filetype=cpp
