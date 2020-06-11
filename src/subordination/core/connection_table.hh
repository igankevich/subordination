#ifndef SUBORDINATION_CORE_CONNECTION_TABLE_HH
#define SUBORDINATION_CORE_CONNECTION_TABLE_HH

#include <memory>
#include <vector>

#include <unistdx/io/fd_type>

#include <subordination/core/connection.hh>

namespace sbn {

    class connection_table {

    private:
        using connection_ptr = std::shared_ptr<connection>;
        using connection_array = std::vector<connection_ptr>;

    public:
        using value_type = connection_array::value_type;
        using reference = connection_array::reference;
        using const_reference = connection_array::const_reference;
        using size_type = int;
        using iterator = connection_ptr*;
        using const_iterator = const connection_ptr*;

    private:
        connection_array _connections;

    public:
        inline iterator begin() noexcept { return this->_connections.data(); }
        inline iterator end() noexcept { return begin() + size(); }
        inline const_iterator begin() const noexcept { return this->_connections.data(); }
        inline const_iterator end() const noexcept { return begin() + size(); }
        inline size_type size() const noexcept { return this->_connections.size(); }
        inline void reserve(size_type n) { this->_connections.reserve(n); }

        inline void emplace(size_type fd, connection_ptr&& ptr) {
            ensure_size(fd);
            this->_connections[fd] = std::move(ptr);
        }

        inline void emplace(size_type fd, const connection_ptr& ptr) {
            ensure_size(fd);
            this->_connections[fd] = ptr;
        }

        inline iterator find(size_type fd) noexcept {
            return (fd < size()) ? (begin() + fd) : end();
        }

        inline const_iterator find(size_type fd) const noexcept {
            return (fd < size()) ? (begin() + fd) : end();
        }

        inline reference operator[](size_type fd) { return this->_connections[fd]; }

        inline const_reference operator[](size_type fd) const {
            return this->_connections[fd];
        }

        inline void erase(size_type fd) {
            if (fd < size()) { this->_connections[fd] = nullptr; }
        }

    private:
        inline void ensure_size(size_type fd) {
            auto n = size();
            if (n == 0) { n = 4096 / sizeof(connection_ptr); }
            while (n <= fd) { n *= 2; }
            this->_connections.resize(n);
        }

    };
}

#endif // vim:filetype=cpp
