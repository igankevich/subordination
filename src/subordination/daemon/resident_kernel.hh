#ifndef SUBORDINATION_DAEMON_RESIDENT_KERNEL_HH
#define SUBORDINATION_DAEMON_RESIDENT_KERNEL_HH

#include <subordination/core/kernel.hh>

namespace sbnd {

    class resident_kernel: public sbn::kernel {

    private:
        struct start_message: public sbn::kernel {};
        struct stop_message: public sbn::kernel {};

    public:
        void start();
        void stop();
        void act() override;
        void react(sbn::kernel_ptr&& k) override;

    protected:
        virtual void on_start();
        virtual void on_stop();
        virtual void on_kernel(sbn::kernel_ptr&& k);

    };

}

#endif // vim:filetype=cpp
