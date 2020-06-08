#ifndef SUBORDINATION_CORE_TRANSACTION_LOG_HH
#define SUBORDINATION_CORE_TRANSACTION_LOG_HH

#include <unistdx/base/types>
#include <unistdx/io/fildes>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/types.hh>

namespace sbn {

    enum class transaction_status: sys::u8 {
        start=1,
        end=2
    };

    class transaction_log {

    public:
        using kernel_array = std::vector<kernel*>;

    private:
        kernel_buffer _buffer{4096};
        sys::fildes _file_descriptor;

    public:
        transaction_log() = default;
        ~transaction_log() = default;
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;
        transaction_log(transaction_log&&) = default;
        transaction_log& operator=(transaction_log&&) = default;

        void write(transaction_status status, kernel* k);
        auto open(const char* filename) -> kernel_array;
        void flush();
        void close();

    private:
        auto recover(const char* filename, sys::offset_type max_offset) -> kernel_array;

    };

}

#endif // vim:filetype=cpp
