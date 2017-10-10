#ifndef BSCHEDULER_PPL_PIPELINE_BASE_HH
#define BSCHEDULER_PPL_PIPELINE_BASE_HH

#include <chrono>
#include <unistdx/base/log_message>

namespace bsc {

	enum struct pipeline_state {
		initial,
		starting,
		started,
		stopping,
		stopped
	};

	class pipeline_base {

	public:
		typedef std::chrono::system_clock clock_type;
		typedef clock_type::time_point time_point;
		typedef clock_type::duration duration;

	protected:
		volatile pipeline_state _state = pipeline_state::initial;
		time_point _start;
		const char* _name = "ppl";
		unsigned _number = 0;

	public:
		pipeline_base() = default;

		virtual
		~pipeline_base() = default;

		pipeline_base(pipeline_base&&) = default;

		pipeline_base(const pipeline_base&) = delete;

		pipeline_base&
		operator=(pipeline_base&) = delete;

		inline void
		setstate(pipeline_state rhs) noexcept {
			this->_state = rhs;
			if (rhs == pipeline_state::starting) {
				this->_start = clock_type::now();
			}
		}

		inline pipeline_state
		state() const noexcept {
			return this->_state;
		}

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
		has_stopped() const noexcept {
			return this->_state == pipeline_state::stopped;
		}

		inline time_point
		start_time_point() const noexcept {
			return this->_start;
		}

		inline const char*
		name() const noexcept {
			return this->_name;
		}

		inline void
		set_name(const char* rhs) noexcept {
			this->_name = rhs;
		}

		inline void
		set_number(unsigned rhs) noexcept {
			this->_number = rhs;
		}

		template <class ... Args>
		inline void
		log(const Args& ... args) {
			sys::log_message(this->_name, args ...);
		}

		inline void
		log_error(const std::exception& err) {
			sys::log_message(this->_name, "error: _", err.what());
		}

	};

}

#endif // vim:filetype=cpp
