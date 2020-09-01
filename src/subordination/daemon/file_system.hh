#ifndef SUBORDINATION_DAEMON_FILE_SYSTEM_HH
#define SUBORDINATION_DAEMON_FILE_SYSTEM_HH

#include <vector>

#include <unistdx/net/socket_address>

namespace sbnd {

    class file_system {

    public:
        using const_path = const char*;
        using address_array = std::vector<sys::socket_address>;

    private:
        const char* _name{""};

    public:
        inline explicit file_system(const char* name): _name(name) {}
        file_system() = default;
        virtual ~file_system() = default;
        file_system(const file_system&) = default;
        file_system& operator=(const file_system&) = default;
        file_system(file_system&&) = default;
        file_system& operator=(file_system&&) = default;

        virtual void
        locate(const_path path, address_array& nodes) const noexcept = 0;

        inline const char* name() const noexcept { return this->_name; }
        inline void name(const char* rhs) noexcept { this->_name = rhs; }

    };

}

#endif // vim:filetype=cpp
