#ifndef SUBORDINATION_CORE_APPLICATION_KERNEL_HH
#define SUBORDINATION_CORE_APPLICATION_KERNEL_HH

#include <cstdint>
#include <string>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/fs/canonical_path>
#include <unistdx/net/socket>

#include <subordination/core/kernel.hh>

namespace sbn {

    class application_kernel: public kernel {

    public:
        typedef kernel kernel_type;
        typedef std::vector<std::string> container_type;

    private:
        container_type _args, _env;
        std::string _error;
        application_type _application = 0;
        sys::canonical_path _workdir;
        sys::user_credentials _credentials;

    public:

        inline
        application_kernel(
            const container_type& args,
            const container_type& env
        ):
        _args(args),
        _env(env),
        _workdir(".")
        {}

        application_kernel() = default;
        virtual ~application_kernel() = default;

        void act() override;

        inline const container_type&
        arguments() const noexcept {
            return this->_args;
        }

        inline const container_type&
        environment() const noexcept {
            return this->_env;
        }

        inline void
        set_error(const std::string& rhs) {
            this->_error = rhs;
        }

        const std::string
        error() const noexcept {
            return this->_error;
        }

        application_type
        application() const noexcept {
            return this->_application;
        }

        inline void
        application(application_type rhs) noexcept {
            this->_application = rhs;
        }

        const sys::canonical_path&
        workdir() const noexcept {
            return this->_workdir;
        }

        inline void
        workdir(const sys::canonical_path& rhs) {
            this->_workdir = rhs;
        }

        inline void credentials(const sys::user_credentials& rhs) noexcept {
            this->_credentials = rhs;
        }

        void
        write(sys::pstream& out) const override;

        void
        read(sys::pstream& in) override;

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("app", args ...);
        }

    };

}

#endif // vim:filetype=cpp
