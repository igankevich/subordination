#ifndef FACTORY_TYPE_REGISTRY_HH
#define FACTORY_TYPE_REGISTRY_HH

#include <vector>
#include <iosfwd>

#include "type.hh"
#include "type_error.hh"

namespace factory {

	class Types {

	public:
		typedef Type::id_type id_type;
		typedef std::vector<Type> container_type;
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
		register_type(Type type);

		template<class X>
		void
		register_type() {
			this->register_type({
				this->generate_id(),
				[] (sys::pstream& in) -> void* {
					X* kernel = new X;
					kernel->read(in);
					return kernel;
				},
				typeid(X)
			});
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Types& rhs);

		void*
		read_object(sys::pstream& packet);

	private:

		inline id_type
		generate_id() noexcept {
			return ++this->_counter;
		}

	};

	std::ostream&
	operator<<(std::ostream& out, const Types& rhs);

}

#endif // FACTORY_TYPE_REGISTRY_HH vim:filetype=cpp
