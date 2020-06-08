#ifndef SUBORDINATION_DAEMON_APPLICATION_KERNEL_HH
#define SUBORDINATION_DAEMON_APPLICATION_KERNEL_HH

#include <cstdint>
#include <string>
#include <vector>

#include <unistdx/base/log_message>
#include <unistdx/fs/path>
#include <unistdx/net/socket>

#include <subordination/core/kernel.hh>

namespace sbnd {

    class application_kernel: public sbn::kernel {

    public:
        using container_type = std::vector<std::string>;

    private:
        container_type _args, _env;
        std::string _error;
        sbn::application::id_type _application = 0;
        sys::path _working_directory;
        sys::user_credentials _credentials;

    public:

        inline
        application_kernel(
            const container_type& args,
            const container_type& env
        ):
        _args(args),
        _env(env),
        _working_directory(".")
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

        sbn::application::id_type
        application() const noexcept {
            return this->_application;
        }

        inline void
        application(sbn::application::id_type rhs) noexcept {
            this->_application = rhs;
        }

        const sys::path&
        working_directory() const noexcept {
            return this->_working_directory;
        }

        inline void
        working_directory(const sys::path& rhs) {
            this->_working_directory = rhs;
        }

        inline const sys::user_credentials& credentials() const noexcept {
            return this->_credentials;
        }

        inline void credentials(const sys::user_credentials& rhs) noexcept {
            this->_credentials = rhs;
        }

        void write(sbn::kernel_buffer& out) const override;
        void read(sbn::kernel_buffer& in) override;

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("app", args ...);
        }

    };

}

#endif // vim:filetype=cpp
