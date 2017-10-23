#ifndef BSCHEDULER_KERNEL_KERNEL_TYPE_REGISTRY_HH
#define BSCHEDULER_KERNEL_KERNEL_TYPE_REGISTRY_HH

#include <vector>
#include <iosfwd>

#include <bscheduler/kernel/kernel.hh>
#include <bscheduler/kernel/kernel_type.hh>
#include <bscheduler/kernel/kernel_type_error.hh>

namespace bsc {

	class kernel_type_registry {

	public:
		typedef kernel_type::id_type id_type;
		typedef std::vector<kernel_type> container_type;
		typedef container_type::iterator iterator;
		typedef container_type::const_iterator const_iterator;

	private:
		container_type _types;
		id_type _counter = 0;

	public:
		const_iterator
		find(id_type id) const noexcept;

		const_iterator
		find(std::type_index idx) const noexcept;

		inline const_iterator
		begin() const noexcept {
			return this->_types.begin();
		}

		inline const_iterator
		end() const noexcept {
			return this->_types.end();
		}

		void
		register_type(kernel_type type);

		template<class X>
		void
		register_type() {
			this->register_type({
				this->generate_id(),
				[] (sys::pstream& in) -> kernel* {
					X* k = new X;
					k->read(in);
					return k;
				},
				typeid(X)
			});
		}

		friend std::ostream&
		operator<<(std::ostream& out, const kernel_type_registry& rhs);

		kernel*
		read_object(sys::pstream& packet);

	private:

		inline id_type
		generate_id() noexcept {
			return ++this->_counter;
		}

	};

	std::ostream&
	operator<<(std::ostream& out, const kernel_type_registry& rhs);

	extern kernel_type_registry types;

	template<class X>
	inline void
	register_type() {
		types.register_type<X>();
	}

	inline void
	register_type(const kernel_type& rhs) {
		types.register_type(rhs);
	}

}

#endif // vim:filetype=cpp
