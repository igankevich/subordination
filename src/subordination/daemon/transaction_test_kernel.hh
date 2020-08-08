#ifndef SUBORDINATION_DAEMON_TRANSACTION_TEST_KERNEL_HH
#define SUBORDINATION_DAEMON_TRANSACTION_TEST_KERNEL_HH

#include <subordination/core/kernel.hh>

namespace sbnd {

    class Transaction_test_kernel: public sbn::kernel {

    private:
        enum class State: sys::u8 {
            Initial,
            Running,
            Checking,
        };
        using record_array = sbn::transaction_log::record_array;

    private:
        sbn::application _application;
        std::vector<sbn::exit_code> _exit_codes;
        record_array _records;

    public:
        Transaction_test_kernel() = default;
        inline explicit Transaction_test_kernel(sbn::application a):
        _application(std::move(a)) {}
        void act() override;
        void react(sbn::kernel_ptr&& k) override;
        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

        inline const std::vector<sbn::exit_code>& exit_codes() const noexcept {
            return this->_exit_codes;
        }

    };

    class Transaction_gather_superior: public sbn::kernel {

    public:
        using record_array = sbn::transaction_log::record_array;

    private:
        record_array _records;
        sbn::application::id_type _application_id{};
        std::size_t _nkernels = 0;

    public:
        void act() override;
        void react(sbn::kernel_ptr&& k) override;

        inline sbn::application::id_type application_id() const noexcept {
            return this->_application_id;
        }

        inline void application_id(sbn::application::id_type rhs) noexcept {
            this->_application_id = rhs;
        }

        inline record_array& records() noexcept { return this->_records; }

    };

    class Transaction_gather_subordinate: public sbn::kernel {

    public:
        using record_array = sbn::transaction_log::record_array;

    private:
        record_array _records;
        sbn::application::id_type _application_id{};

    public:
        void act() override;
        void write(sbn::kernel_buffer& out) const;
        void read(sbn::kernel_buffer& in);

        inline record_array& records() noexcept { return this->_records; }

        inline sbn::application::id_type application_id() const noexcept {
            return this->_application_id;
        }

        inline void application_id(sbn::application::id_type rhs) noexcept {
            this->_application_id = rhs;
        }

    };

}

#endif // vim:filetype=cpp
