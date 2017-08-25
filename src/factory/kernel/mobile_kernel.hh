#ifndef FACTORY_KERNEL_MOBILE_KERNEL_HH
#define FACTORY_KERNEL_MOBILE_KERNEL_HH

#include "basic_kernel.hh"
#include "kernel_header.hh"

namespace factory {

	class Mobile_kernel: public Basic_kernel, public Kernel_header {

	public:
		typedef uint64_t id_type;

	private:
		id_type _id = no_id();

	public:
		virtual void
		read(sys::pstream& in);

		virtual void
		write(sys::pstream& out);

		inline id_type
		id() const noexcept {
			return this->_id;
		}

		inline void
		id(id_type rhs) noexcept {
			this->_id = rhs;
		}

		inline bool
		has_id() const noexcept {
			return this->_id != no_id();
		}

		inline void
		set_id(id_type rhs) noexcept {
			this->_id = rhs;
		}

		inline bool
		operator==(const Mobile_kernel& rhs) const noexcept {
			return this == &rhs || (
				this->id() == rhs.id()
				&& this->has_id()
				&& rhs.has_id()
			);
		}

		inline bool
		operator!=(const Mobile_kernel& rhs) const noexcept {
			return !this->operator==(rhs);
		}

		static constexpr id_type
		no_id() noexcept {
			return 0;
		}

		inline uint64_t
		unique_id() const noexcept {
			return this->has_id() ? this->id() : uint64_t(this);
		}


	};

}

#endif // FACTORY_KERNEL_MOBILE_KERNEL_HH vim:filetype=cpp
