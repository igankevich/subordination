#ifndef SUBORDINATION_CORE_TRANSACTION_LOG_HH
#define SUBORDINATION_CORE_TRANSACTION_LOG_HH

#include <limits>
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
        transaction_status status{};
        pipeline::index_type pipeline_index;
        kernel_ptr k;
        transaction_record() = default;
        inline transaction_record(transaction_status s, pipeline::index_type i, kernel_ptr&& kk):
        status(s), pipeline_index(i), k(std::move(kk)) {}
    };

    kernel_buffer& operator<<(kernel_buffer& out, const transaction_record& rhs);
    kernel_buffer& operator>>(kernel_buffer& in, transaction_record& rhs);

    class transaction_log {

    public:
        using pipeline_array = std::vector<pipeline*>;
        using record_array = std::vector<transaction_record>;
        using clock_type = std::chrono::system_clock;
        using duration = clock_type::duration;
        using time_point = clock_type::time_point;

    private:
        using mutex_type = std::mutex;
        using lock_type = std::lock_guard<mutex_type>;

    private:
        class offset_guard {
        private:
            sys::fildes _fd;
            sys::offset_type _old_offset;
        public:
            inline explicit offset_guard(sys::fildes& fd): _fd(fd), _old_offset(fd.offset()) {}
            inline ~offset_guard() { this->_fd.offset(this->_old_offset); }
            inline sys::offset_type old_offset() const noexcept { return this->_old_offset; }
        };

    public:
        class sentry {
        private:
            const transaction_log& _transactions;
        public:
            inline explicit sentry(const transaction_log& rhs): _transactions(rhs) {
                this->_transactions._mutex.lock();
            }
            inline ~sentry() { this->_transactions._mutex.unlock(); }
        };

    private:
        kernel_buffer _buffer{4096};
        sys::fildes _file_descriptor;
        pipeline_array _pipelines;
        pipeline* _timer_pipeline{};
        mutable mutex_type _mutex;
        std::size_t _max_records = std::numeric_limits<std::size_t>::max();
        std::size_t _actual_records = 0;
        duration _recover_after{duration::zero()};

    public:
        transaction_log();
        ~transaction_log() noexcept;
        transaction_log(const transaction_log&) = delete;
        transaction_log& operator=(const transaction_log&) = delete;
        transaction_log(transaction_log&&) = default;
        transaction_log& operator=(transaction_log&&) = default;

        kernel_ptr write(transaction_record record);

        void open(const char* filename);
        void flush();
        void close();
        record_array select(application::id_type id);
        void recover(record_array& records);

        inline void pipelines(const pipeline_array& rhs) {
            this->_pipelines = rhs;
            update_pipeline_indices();
        }

        inline void pipelines(pipeline_array&& rhs) {
            this->_pipelines = std::move(rhs);
            update_pipeline_indices();
        }

        inline const pipeline_array& pipelines() const noexcept {
            return this->_pipelines;
        }

        inline void timer_pipeline(pipeline* rhs) noexcept { this->_timer_pipeline = rhs; }
        inline void types(kernel_type_registry* rhs) noexcept { this->_buffer.types(rhs); }
        inline std::size_t max_records() const noexcept { return this->_max_records; }
        inline void max_records(std::size_t rhs) noexcept { this->_max_records = rhs; }
        inline std::size_t actual_records() const noexcept { return this->_actual_records; }
        inline void actual_records(std::size_t rhs) noexcept { this->_actual_records = rhs; }
        inline sentry guard() noexcept { return sentry(*this); }
        inline sentry guard() const noexcept { return sentry(*this); }
        inline void recover_after(duration rhs) noexcept { this->_recover_after = rhs; }
        inline duration recover_after() const noexcept { return this->_recover_after; }

        static void plug_parents(record_array& records);

    private:
        void recover(const char* filename, sys::offset_type max_offset);
        void update_pipeline_indices();
        record_array read_records(sys::fildes& fd,
                                  sys::offset_type max_offset,
                                  kernel_buffer& buf);

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("transactions", args ...);
        }

    };

    class transaction_kernel: public kernel {

    public:
        using record_array = std::vector<transaction_record>;

    private:
        record_array _records;
        transaction_log* _transactions{};

    public:
        void act() override;

        inline void records(record_array&& rhs) noexcept {
            this->_records = std::move(rhs);
        }

        inline void transactions(transaction_log* rhs) noexcept {
            this->_transactions = rhs;
        }

    };

}

#endif // vim:filetype=cpp
