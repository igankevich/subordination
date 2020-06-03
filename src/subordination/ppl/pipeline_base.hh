#ifndef SUBORDINATION_PPL_PIPELINE_BASE_HH
#define SUBORDINATION_PPL_PIPELINE_BASE_HH

#include <unistdx/base/log_message>

#include <subordination/kernel/foreign_kernel.hh>

namespace sbn {

    enum struct pipeline_state {
        initial,
        starting,
        started,
        stopping,
        stopped
    };

    class pipeline_base {

    protected:
        volatile pipeline_state _state = pipeline_state::initial;
        const char* _name = "ppl";

    public:
        pipeline_base() = default;
        virtual ~pipeline_base() = default;
        pipeline_base(pipeline_base&&) = default;
        pipeline_base(const pipeline_base&) = delete;
        pipeline_base& operator=(pipeline_base&) = delete;

        inline pipeline_state state() const noexcept { return this->_state; }

        inline bool
        is_starting() const noexcept {
            return this->_state == pipeline_state::starting;
        }

        inline bool
        has_started() const noexcept {
            return this->_state == pipeline_state::started;
        }

        inline bool
        is_running() const noexcept {
            return this->_state == pipeline_state::starting ||
                   this->_state == pipeline_state::started;
        }

        inline bool
        is_stopping() const noexcept {
            return this->_state == pipeline_state::stopping;
        }

        inline bool
        stopped() const noexcept {
            return this->_state == pipeline_state::stopped;
        }

        inline const char* name() const noexcept { return this->_name; }
        inline void name(const char* rhs) noexcept { this->_name = rhs; }

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message(this->_name, args ...);
        }

        inline void
        log_error(const std::exception& err) const {
            sys::log_message(this->_name, "error: _", err.what());
        }

    protected:

        inline void setstate(pipeline_state rhs) noexcept { this->_state = rhs; }

    };

    class pipeline: public pipeline_base {

    public:
        virtual void send(kernel* k) = 0;
        virtual void forward(foreign_kernel* k);

    };

}

#endif // vim:filetype=cpp
