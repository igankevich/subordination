#ifndef SUBORDINATION_DAEMON_RESIDENT_KERNEL_HH
#define SUBORDINATION_DAEMON_RESIDENT_KERNEL_HH

#include <subordination/api.hh>

namespace sbn {

    class resident_kernel: public sbn::kernel {

    private:
        struct start_message: public sbn::kernel {};
        struct stop_message: public sbn::kernel {};

    public:

        inline void
        start() {
            send(new start_message, this);
        }

        inline void
        stop() {
            send(new stop_message, this);
        }

        void
        act() override;

        void
        react(sbn::kernel* k) override;

    protected:

        virtual void
        on_start();

        virtual void
        on_stop();

        virtual void
        on_kernel(sbn::kernel* k);

    };

}

#endif // vim:filetype=cpp
