#ifndef EXAMPLES_SPEC_TIMESTAMP_HH
#define EXAMPLES_SPEC_TIMESTAMP_HH

#include <iosfwd>

#include <subordination/core/kernel_buffer.hh>

namespace spec {

    class Timestamp {

    private:
        std::uint64_t _timestamp{};

    public:

        Timestamp() = default;
        ~Timestamp() = default;
        Timestamp(const Timestamp&) = default;
        Timestamp& operator=(const Timestamp&) = default;
        Timestamp(Timestamp&&) = default;
        Timestamp& operator=(Timestamp&&) = default;

        inline std::uint64_t get() const noexcept { return this->_timestamp; }

        inline bool operator==(const Timestamp& rhs) const noexcept {
            return this->_timestamp == rhs._timestamp;
        }

        inline bool operator!=(const Timestamp& rhs) const noexcept {
            return !this->operator==(rhs);
        }

        inline bool operator<(const Timestamp& rhs) const noexcept {
            return this->_timestamp < rhs._timestamp;
        }

        friend std::istream&
        operator>>(std::istream& in, Timestamp& rhs);

        friend std::ostream&
        operator<<(std::ostream& out, const Timestamp& rhs) {
            return out << rhs._timestamp;
        }

        friend sbn::kernel_buffer&
        operator>>(sbn::kernel_buffer& in, Timestamp& rhs) {
            return in >> rhs._timestamp;
        }

        friend sbn::kernel_buffer&
        operator<<(sbn::kernel_buffer& out, const Timestamp& rhs) {
            return out << rhs._timestamp;
        }

    };

    std::istream&
    operator>>(std::istream& in, Timestamp& rhs);

}

namespace std {
    template <>
    class hash<spec::Timestamp>: public hash<std::uint64_t> {
    public:
        size_t operator()(spec::Timestamp t) const noexcept {
            return this->hash<std::uint64_t>::operator()(t.get());
        }
    };
}

#endif // vim:filetype=cpp
