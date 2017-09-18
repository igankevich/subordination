#ifndef EXAMPLES_FACTORY_HIERARCHY_KERNEL_HH
#define EXAMPLES_FACTORY_HIERARCHY_KERNEL_HH

#include <factory/api.hh>

namespace factory {

	class hierarchy_kernel: public factory::api::Kernel {

	private:
		uint32_t _weight = 0;

	public:

		hierarchy_kernel() = default;

		inline explicit
		hierarchy_kernel(uint32_t weight):
		_weight(weight)
		{}

		inline void
		weight(uint32_t rhs) noexcept {
			this->_weight = rhs;
		}

		inline uint32_t
		weight() const noexcept {
			return this->_weight;
		}

		void
		write(sys::pstream& out) override;

		void
		read(sys::pstream& in) override;

	};

}

#endif // vim:filetype=cpp
