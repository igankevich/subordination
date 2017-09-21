#ifndef FACTORY_KERNEL_KERNEL_HH
#define FACTORY_KERNEL_KERNEL_HH

#include <unistdx/net/pstream>
#include <memory>

#include "mobile_kernel.hh"

namespace asc {

	struct kernel: public mobile_kernel {

		typedef mobile_kernel base_kernel;
		using mobile_kernel::id_type;

		inline const kernel*
		principal() const {
			return this->isset(kernel_flag::principal_is_id)
				? nullptr : this->_principal;
		}

		inline kernel*
		principal() {
			return this->_principal;
		}

		inline id_type
		principal_id() const {
			return this->_principal_id;
		}

		inline void
		set_principal_id(id_type id) {
			this->_principal_id = id;
			this->setf(kernel_flag::principal_is_id);
		}

		inline void
		principal(kernel* rhs) {
			this->_principal = rhs;
			this->unsetf(kernel_flag::principal_is_id);
		}

		inline const kernel*
		parent() const {
			return this->_parent;
		}

		inline kernel*
		parent() {
			return this->_parent;
		}

		inline id_type
		parent_id() const {
			return this->_parent_id;
		}

		inline void
		parent(kernel* p) {
			this->_parent = p;
			this->unsetf(kernel_flag::parent_is_id);
		}

		inline size_t
		hash() const {
			return this->_principal && this->_principal->has_id()
				? this->_principal->id()
				: size_t(this->_principal) / alignof(size_t);
		}

		inline bool
		moves_upstream() const noexcept {
			return this->return_code() == exit_code::undefined &&
				!this->_principal &&
				this->_parent;
		}

		inline bool
		moves_downstream() const noexcept {
			return this->return_code() != exit_code::undefined &&
				this->_principal &&
				this->_parent;
		}

		inline bool
		moves_somewhere() const noexcept {
			return this->return_code() == exit_code::undefined &&
				this->_principal &&
				this->_parent;
		}

		inline bool
		moves_everywhere() const noexcept {
			return !this->_principal && !this->_parent;
		}

		void
		read(sys::pstream& in) override;

		void
		write(sys::pstream& out) override;

		virtual void
		act();

		virtual void
		react(kernel* child);

		virtual void
		error(kernel* rhs);

		friend std::ostream&
		operator<<(std::ostream& out, const kernel& rhs);

	public:

		/// New API

		inline kernel*
		call(kernel* rhs) noexcept {
			rhs->parent(this);
			return rhs;
		}

		inline kernel*
		carry_parent(kernel* rhs) noexcept {
			rhs->parent(this);
			rhs->setf(kernel_flag::carries_parent);
			return rhs;
		}

		inline void
		return_to_parent(exit_code ret = exit_code::success) noexcept {
			return_to(_parent, ret);
		}

		inline void
		return_to(kernel* rhs, exit_code ret = exit_code::success) noexcept {
			this->principal(rhs);
			this->return_code(ret);
		}

		inline void
		recurse() noexcept {
			this->principal(this);
		}

		template<class It>
		void
		mark_as_deleted(It result) noexcept {
			if (!this->isset(kernel_flag::deleted)) {
				this->setf(kernel_flag::deleted);
				if (this->_parent) {
					this->_parent->mark_as_deleted(result);
				}
				*result = std::unique_ptr<kernel>(this);
				++result;
			}
		}

	private:

		union {
			kernel* _parent = nullptr;
			id_type _parent_id;
		};
		union {
			kernel* _principal = nullptr;
			id_type _principal_id;
		};

	};

	std::ostream&
	operator<<(std::ostream& out, const kernel& rhs);

}

#endif // vim:filetype=cpp
