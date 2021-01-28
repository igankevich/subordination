#ifndef SUBORDINATION_CORE_PIPELINE_BASE_HH
#define SUBORDINATION_CORE_PIPELINE_BASE_HH

#include <unistdx/base/log_message>

#include <subordination/core/foreign_kernel.hh>

namespace sbn {

    class pipeline_base {

    public:
        enum struct states {initial, starting, started, stopping, stopped};

    protected:
        states _state{states::initial};
        const char* _name = "ppl";

    public:
        pipeline_base() = default;
        ~pipeline_base() = default;
        pipeline_base(const pipeline_base&) = default;
        pipeline_base& operator=(const pipeline_base&) = default;
        pipeline_base(pipeline_base&&) = default;
        pipeline_base& operator=(pipeline_base&&) = default;

        inline states state() const noexcept { return this->_state; }
        inline bool starting() const noexcept { return this->_state == states::starting; }
        inline bool started() const noexcept { return this->_state == states::started; }

        inline bool
        running() const noexcept {
            return this->_state == states::starting ||
                   this->_state == states::started;
        }

        inline bool stopping() const noexcept { return this->_state == states::stopping; }
        inline bool stopped() const noexcept { return this->_state == states::stopped; }

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

        inline void setstate(states rhs) noexcept { this->_state = rhs; }

    };

    const char* to_string(pipeline_base::states rhs);
    std::ostream& operator<<(std::ostream& out, pipeline_base::states rhs);

    class pipeline: public pipeline_base {

    public:
        using index_type = sys::u32;

    private:
        index_type _index{};

    public:
        pipeline() = default;
        virtual ~pipeline() = default;
        pipeline(const pipeline&) = delete;
        pipeline& operator=(const pipeline&) = delete;
        pipeline(pipeline&&) = delete;
        pipeline& operator=(pipeline&&) = delete;

        virtual void send(kernel_ptr&& k) = 0;
        virtual void forward(kernel_ptr&& k);
        virtual void recover(kernel_ptr&& k);

        inline index_type index() const noexcept { return this->_index; }
        inline void index(index_type rhs) noexcept { this->_index = rhs; }

    };

}

#endif // vim:filetype=cpp
