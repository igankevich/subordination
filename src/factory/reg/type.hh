#ifndef FACTORY_TYPE_HH
#define FACTORY_TYPE_HH

#include <typeinfo>
#include <typeindex>
#include <iosfwd>
#include <functional>

#include <unistdx/net/pstream>

namespace factory {

	class Type {

	public:
		/// A portable type id
		typedef uint16_t id_type;
		typedef std::function<void* (sys::pstream&)> read_type;

	private:
		id_type _id;
		read_type _read;
		std::type_index _index;

	public:
		inline
		Type(id_type id, read_type f, std::type_index idx) noexcept:
		_id(id),
		_read(f),
		_index(idx)
		{}

		inline
		Type(read_type f, std::type_index idx) noexcept:
		_id(0),
		_read(f),
		_index(idx)
		{}

		inline void*
		read(sys::pstream& in) const {
			return this->_read(in);
		}

		inline std::type_index
		index() const noexcept {
			return this->_index;
		}

		inline id_type
		id() const noexcept {
			return this->_id;
		}

		inline void
		setid(id_type rhs) noexcept {
			this->_id = rhs;
		}

		inline const char*
		name() const noexcept {
			return this->_index.name();
		}

		inline explicit
		operator bool() const noexcept {
			return this->_id != 0;
		}

		inline bool
		operator !() const noexcept {
			return this->_id == 0;
		}

		inline bool
		operator==(const Type& rhs) const noexcept {
			return this->_id == rhs._id;
		}

		inline bool
		operator!=(const Type& rhs) const noexcept {
			return !this->operator==(rhs);
		}

		inline bool
		operator<(const Type& rhs) const noexcept {
			return this->_id < rhs._id;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Type& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const Type& rhs);

}

namespace std {

	template<>
	struct hash<factory::Type>: public std::hash<factory::Type::id_type> {

		typedef size_t result_type;
		typedef factory::Type argument_type;

		size_t
		operator()(const factory::Type& rhs) const noexcept {
			return std::hash<factory::Type::id_type>::operator()(rhs.id());
		}

	};

}

#endif // FACTORY_TYPE_HH
