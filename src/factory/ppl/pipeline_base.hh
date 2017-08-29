#ifndef FACTORY_PPL_PIPELINE_BASE_HH
#define FACTORY_PPL_PIPELINE_BASE_HH

namespace factory {

	enum struct pipeline_state {
		initial,
		starting,
		started,
		stopping,
		stopped
	};

	struct pipeline_base {

		pipeline_base() = default;
		virtual ~pipeline_base() = default;
		pipeline_base(pipeline_base&&) = default;
		pipeline_base(const pipeline_base&) = delete;
		pipeline_base& operator=(pipeline_base&) = delete;

		inline void
		setstate(pipeline_state rhs) noexcept {
			this->_state = rhs;
		}

		inline bool
		is_stopped() const noexcept {
			return this->_state == pipeline_state::stopped;
		}

		inline bool
		is_stopping() const noexcept {
			return this->_state == pipeline_state::stopping;
		}

		inline bool
		is_started() const noexcept {
			return this->_state == pipeline_state::started;
		}

	protected:

		volatile pipeline_state _state = pipeline_state::initial;

	};

}

#endif // vim:filetype=cpp
