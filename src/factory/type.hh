#ifndef FACTORY_TYPE_HH
#define FACTORY_TYPE_HH

#include <unordered_map>
#include <set>
#include <algorithm>
#include <typeinfo>
#include <typeindex>

#include <sys/packetstream.hh>
#include <factory/error.hh>

namespace factory {

	using components::Bad_kernel;
	using components::Error;

	struct Type {

		/// A portable type id
		typedef uint16_t id_type;
		typedef std::function<void* (sys::packetstream&)> read_type;

		Type(id_type id, read_type f, std::type_index idx) noexcept:
		_id(id),
		_read(f),
		_index(idx)
		{}

		Type(read_type f, std::type_index idx) noexcept:
		_id(0),
		_read(f),
		_index(idx)
		{}

		explicit
		Type(id_type id) noexcept:
		_id(id),
		_read(),
		_index(typeid(void))
		{}

		void*
		read(sys::packetstream& in) const {
			return _read(in);
		}

		std::type_index
		index() const noexcept {
			return _index;
		}

		id_type
		id() const noexcept {
			return _id;
		}

		void
		setid(id_type rhs) noexcept {
			_id = rhs;
		}

		const char*
		name() const noexcept {
			return _index.name();
		}

		explicit
		operator bool() const noexcept {
			return this->_id != 0;
		}

		bool
		operator !() const noexcept {
			return this->_id == 0;
		}

		bool
		operator==(const Type& rhs) const noexcept {
			return this->_id == rhs._id;
		}

		bool
		operator!=(const Type& rhs) const noexcept {
			return !operator==(rhs);
		}

		bool
		operator<(const Type& rhs) const noexcept {
			return _id < rhs._id;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Type& rhs) {
			return out << rhs.name() << '(' << rhs.id() << ')';
		}

	private:

		id_type _id;
		read_type _read;
		std::type_index _index;

	};

	struct Types {

		typedef Type::id_type id_type;

		const Type*
		lookup(id_type id) const noexcept {
			auto result = _types.find(Type(id));
			return result == _types.end() ? nullptr : &*result;
		}

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
			if (type) {
				if (const Type* existing_type = this->lookup(type.id())) {
					std::stringstream msg;
					msg << "'" << type << "' and '" << *existing_type
						<< "' have the same type identifiers.";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
			} else {
				type.setid(generate_id());
			}
			_types.emplace(type);
		}

		template<class X>
		void
		register_type() {
			_types.emplace(
				generate_id(),
				[] (sys::packetstream& in) -> void* {
					X* kernel = new X;
					kernel->read(in);
					return kernel;
				},
				typeid(X)
			);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Types& rhs) {
			std::ostream_iterator<Entry> it(out, "\n");
			std::copy(rhs._types.begin(), rhs._types.end(), it);
			return out;
		}

		static void*
		read_object(Types& types, sys::packetstream& packet)
		throw(Bad_kernel)
		{
			id_type id;
			packet >> id;
			const Type* type = types.lookup(id);
			if (type == nullptr) {
				throw Bad_kernel(
					"bad kernel type when reading from Kernel_stream",
					{__FILE__, __LINE__, __func__},
					id
				);
			}
			void* kernel = nullptr;
			try {
				kernel = type->read(packet);
			} catch (std::bad_alloc& err) {
				throw Bad_kernel(
					err.what(),
					{__FILE__, __LINE__, __func__},
					id
				);
			}
			return kernel;
		}

		/// @deprecated
		template<class Func>
		static void
		read_object(Types& types, sys::packetstream& packet, Func callback) {
			id_type id;
			packet >> id;
			const Type* type = types.lookup(id);
			if (type == nullptr) {
				throw Bad_kernel(
					"bad kernel type when reading from Kernel_stream",
					{__FILE__, __LINE__, __func__},
					id
				);
			}
			void* kernel = nullptr;
			try {
				kernel = type->read(packet);
			} catch (std::bad_alloc& err) {
				throw Bad_kernel(
					err.what(),
					{__FILE__, __LINE__, __func__},
					id
				);
			}
			callback(kernel);
		}

	private:

		id_type
		generate_id() noexcept {
			return ++_counter;
		}

		struct Entry {
			Entry(const Type& rhs): _type(rhs) {}

			friend std::ostream&
			operator<<(std::ostream& out, const Entry& rhs) {
				return out << "/type/" << rhs._type.id() << '/' << rhs._type;
			}

		private:
			const Type& _type;
		};

		std::set<Type> _types;
		id_type _counter = 0;
	};

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
