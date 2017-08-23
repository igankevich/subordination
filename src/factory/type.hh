#ifndef FACTORY_TYPE_HH
#define FACTORY_TYPE_HH

#include <unordered_map>
#include <set>
#include <algorithm>
#include <typeinfo>
#include <typeindex>
#include <sstream>
#include <iosfwd>
#include <functional>
#include <mutex>

#include <unistdx/net/pstream>
#include <factory/error.hh>
#include <factory/kernel_error.hh>

namespace factory {

	struct Type {

		/// A portable type id
		typedef uint16_t id_type;
		typedef std::function<void* (sys::pstream&)> read_type;

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

		inline explicit
		Type(id_type id) noexcept:
		_id(id),
		_read(),
		_index(typeid(void))
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

	private:

		id_type _id;
		read_type _read;
		std::type_index _index;

	};

	std::ostream&
	operator<<(std::ostream& out, const Type& rhs);

	class Type_error: public Error {

	private:
		Type _tp1, _tp2;

	public:
		inline
		Type_error(Type tp1, Type tp2, const Error_location& loc):
		Error(loc),
		_tp1(tp1),
		_tp2(tp2)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Type_error& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const Type_error& rhs);

	struct Types {

		typedef Type::id_type id_type;

		const Type*
		lookup(id_type id) const noexcept;

		const Type*
		lookup(std::type_index idx) const noexcept {
			auto result = std::find_if(
				_types.begin(),
				_types.end(),
				[&idx] (const Type& rhs) {
					return rhs.index() == idx;
				}
			);
			return result == _types.end() ? nullptr : &*result;
		}

		void
		register_type(Type type) {
			if (const Type* existing_type = this->lookup(type.index())) {
				FACTORY_THROW(Type_error, type, *existing_type);
			}
			if (type) {
				if (const Type* existing_type = this->lookup(type.id())) {
					FACTORY_THROW(Type_error, type, *existing_type);
				}
			} else {
				type.setid(generate_id());
			}
			_types.emplace(type);
		}

		template<class X>
		void
		register_type() {
			const std::type_index idx = typeid(X);
			this->_types.emplace(
				generate_id(),
				[] (sys::pstream& in) -> void* {
					X* kernel = new X;
					kernel->read(in);
					return kernel;
				},
				idx
			);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Types& rhs);

		static void*
		read_object(Types& types, sys::pstream& packet) {
			id_type id;
			packet >> id;
			const Type* type = types.lookup(id);
			if (type == nullptr) {
				throw Kernel_error("unknown kernel type", id);
			}
			return type->read(packet);
		}

		/// @deprecated
		template<class Func>
		static void
		read_object(Types& types, sys::pstream& packet, Func callback) {
			id_type id;
			packet >> id;
			const Type* type = types.lookup(id);
			if (type == nullptr) {
				throw Kernel_error("bad kernel type", id);
			}
			callback(type->read(packet));
		}

	private:

		id_type
		generate_id() noexcept {
			return ++_counter;
		}


		std::set<Type> _types;
		id_type _counter = 0;
	};

	std::ostream&
	operator<<(std::ostream& out, const Types& rhs);

	template<class T>
	struct Instances {

		typedef T kernel_type;
		typedef typename kernel_type::id_type id_type;

		kernel_type*
		lookup(id_type id) {
			std::unique_lock<std::mutex> lock(_mutex);
			auto result = _instances.find(id);
			return result == _instances.end() ? nullptr : result->second;
		}

		void
		register_instance(kernel_type* inst) {
			std::unique_lock<std::mutex> lock(_mutex);
			_instances[inst->id()] = inst;
		}

		void
		free_instance(kernel_type* inst) {
			std::unique_lock<std::mutex> lock(_mutex);
			_instances.erase(inst->id());
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Instances& rhs) {
			std::unique_lock<std::mutex> lock(rhs._mutex);
			std::ostream_iterator<Entry> it(out, "\n");
			std::copy(rhs._instances.cbegin(), rhs._instances.cend(), it);
			return out;
		}

	private:

		struct Entry {
			Entry(const std::pair<const id_type, kernel_type*>& k): _kernel(k.second) {}

			friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
				return out
					<< "/instance/"
					<< rhs._kernel->id()
					<< '='
					<< rhs.safe_type_name();
			}

		private:

			const char* safe_type_name() const {
				return !_kernel->type()
					? "_identifiable"
					: _kernel->type().name();
			}

			kernel_type* _kernel;
		};

		std::unordered_map<id_type, kernel_type*> _instances;
		mutable std::mutex _mutex;
	};

}

namespace std {

	template<>
	struct hash<factory::Type> {

		typedef size_t result_type;
		typedef factory::Type argument_type;

		size_t
		operator()(const factory::Type& rhs) const noexcept {
			return rhs.id();
		}

	};

}

#endif // FACTORY_TYPE_HH
