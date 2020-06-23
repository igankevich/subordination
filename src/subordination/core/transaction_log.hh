#ifndef SUBORDINATION_CORE_TRANSACTION_LOG_HH
#define SUBORDINATION_CORE_TRANSACTION_LOG_HH

#include <mutex>

#include <unistdx/base/types>
#include <unistdx/io/fildes>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_buffer.hh>
#include <subordination/core/pipeline_base.hh>
#include <subordination/core/types.hh>

namespace sbn {

    enum class transaction_status: sys::u8 {start=1, end=2};

    kernel_buffer& operator<<(kernel_buffer& out, transaction_status rhs);

    struct transaction_record {
        transaction_status status;
        pipeline::index_type pipeline_index;
        kernel_ptr k;
        transaction_record() = default;
        inline transaction_record(transaction_status s, pipeline::index_type i, kernel_ptr&& kk):
        status(s), pipeline_index(i), k(std::move(kk)) {}
    };

    kernel_buffer& operator<<(kernel_buffer& out, const transaction_record& rhs);

    class transaction_log {

    public:
        using pipeline_array = std::vector<pipeline*>;

    private:
        using mutex_type = std::mutex;
        using lock_type = std::lock_guard<mutex_type>;

    private:
        kernel_buffer _buffer{4096};
        sys::fildes _file_descriptor;
        pipeline_array _pipelines;
        mutex_type _mutex;

    public:
        transaction_log();
        ~transaction_log();
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;
        transaction_log(transaction_log&&) = default;
        transaction_log& operator=(transaction_log&&) = default;

        kernel_ptr write(transaction_record record);

        void open(const char* filename);
        void flush();
        void close();

        inline void pipelines(const pipeline_array& rhs) {
            this->_pipelines = rhs;
            update_pipeline_indices();
        }

        inline void pipelines(pipeline_array&& rhs) {
            this->_pipelines = std::move(rhs);
            update_pipeline_indices();
        }

        inline void types(kernel_type_registry* rhs) noexcept { this->_buffer.types(rhs); }

    private:
        void recover(const char* filename, sys::offset_type max_offset);
        void update_pipeline_indices();

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("transactions", args ...);
        }

    };

}

#endif // vim:filetype=cpp
