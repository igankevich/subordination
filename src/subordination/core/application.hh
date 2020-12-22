#ifndef SUBORDINATION_CORE_APPLICATION_HH
#define SUBORDINATION_CORE_APPLICATION_HH

#include <iosfwd>
#include <vector>

#include <unistdx/fs/path>
#include <unistdx/io/two_way_pipe>
#include <unistdx/ipc/execute>
#include <unistdx/ipc/identity>
#include <unistdx/net/socket>

#include <subordination/core/types.hh>

namespace sbn {

    class application {

    public:
        using id_type = sys::u64;
        using string_array = std::vector<std::string>;

    private:
        id_type _id = 0;
        sys::uid_type _uid = -1;
        sys::gid_type _gid = -1;
        string_array _args, _env;
        sys::path _working_directory;
        bool _wait_for_completion = false;

        static_assert(sizeof(_uid) <= sizeof(uint32_t), "bad uid_type");
        static_assert(sizeof(_gid) <= sizeof(uint32_t), "bad gid_type");

    public:
        application(const string_array& args, const string_array& env);
        application() = default;
        application(application&& rhs) = default;
        application& operator=(application&& rhs) = default;
        application(const application& rhs) = default;
        application& operator=(const application& rhs) = default;

        inline void working_directory(const sys::path& rhs) {
            this->_working_directory = rhs;
        }

        inline const sys::path& working_directory() const noexcept {
            return this->_working_directory;
        }

        inline void credentials(sys::uid_type uid, sys::gid_type gid) noexcept {
            this->_uid = uid, this->_gid = gid;
        }

        inline void credentials(const sys::user_credentials& rhs) noexcept {
            this->_uid = rhs.uid, this->_gid = rhs.gid;
        }

        inline id_type id() const noexcept { return this->_id; }
        inline sys::uid_type user() const noexcept { return this->_uid; }
        inline sys::gid_type group() const noexcept { return this->_gid; }
        inline const std::string& filename() const noexcept { return this->_args.front(); }

        inline const string_array& arguments() const noexcept {
            return this->_args;
        }

        inline bool wait_for_completion() const noexcept {
            return this->_wait_for_completion;
        }

        inline void wait_for_completion(bool rhs) noexcept {
            this->_wait_for_completion = rhs;
        }

        int execute(const sys::two_way_pipe& pipe) const;

        void write(kernel_buffer& out) const;
        void read(kernel_buffer& in);

        friend std::ostream&
        operator<<(std::ostream& out, const application& rhs);

    };

    std::ostream&
    operator<<(std::ostream& out, const application& rhs);

    kernel_buffer& operator<<(kernel_buffer& out, const application& rhs);
    kernel_buffer& operator>>(kernel_buffer& in, application& rhs);

    application::id_type generate_application_id() noexcept;

    namespace this_application {

        /// Cluster-wide application ID.
        application::id_type id() noexcept;
        void id(application::id_type rhs) noexcept;
        sys::fd_type get_input_fd() noexcept;
        sys::fd_type get_output_fd() noexcept;
        bool standalone() noexcept;

    }

}

#endif // vim:filetype=cpp
