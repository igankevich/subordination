#ifndef FACTORY_TYPE_HH
#define FACTORY_TYPE_HH

#include <unordered_map>

#include <stdx/log.hh>
#include <sysx/packetstream.hh>

namespace factory {

	typedef uint32_t Id;

	const Id ROOT_ID = 0;

	namespace components {

		template<class T>
		struct Types {

			typedef typename T::id_type id_type;

			const T*
			lookup(id_type type_name) const noexcept {
				auto result = this->_types.find(type_name);
				return result == this->_types.end() ? nullptr : &result->second; 
			}

			void
			register_type(T type) {
				const T* existing_type = this->lookup(type.id());
				if (existing_type != nullptr) {
					std::stringstream msg;
					msg << "'" << type << "' and '" << *existing_type
						<< "' have the same type identifiers.";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				this->_types[type.id()] = type;
			}

			template<class Func>
			void
			write_recursively(std::ostream& out, Func write_prefix) const {
				std::for_each(_types.begin(), _types.end(),
					[&out,&write_prefix] (const std::pair<const id_type, T>& rhs) {
						write_prefix(out);
						out << rhs.second.id() << '=' << rhs.second << '\n';
					}
				);
			}
	
			friend std::ostream&
			operator<<(std::ostream& out, const Types& rhs) {
				std::ostream_iterator<Entry> it(out, "\n");
				std::copy(rhs._types.cbegin(), rhs._types.cend(), it);
				return out;
			}
	
		private:

			struct Entry {
				Entry(const std::pair<const id_type, T>& k): _type(k.second) {}

				friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
					return out
						<< "/type/"
						<< rhs._type.id()
						<< " = "
						<< rhs._type;
				}

			private:
				T _type;
			};

			std::unordered_map<id_type, T> _types;
		};

		template<class T>
		struct Type {

			/// A portable type id
			typedef int16_t id_type;
			typedef T kernel_type;
//			typedef std::function<T* ()> construct_type;
			typedef std::function<T* (sysx::packetstream&)> read_type;

			typedef stdx::log<Type> this_log;
	
//			constexpr
//			Type() noexcept:
//				_id(0),
//				_name(),
//				construct(),
//				read()
//			{}
//	
//			constexpr
//			Type(const Type& rhs) noexcept:
//				_id(rhs._id),
//				_name(rhs._name),
//				construct(rhs.construct),
//				read(rhs.read)
//			{}

			constexpr id_type
			id() const noexcept {
				return _id;
			}

			void
			id(id_type rhs) noexcept {
				_id = rhs;
			}

			constexpr const char*
			name() const noexcept {
				return _name.c_str();
			}

			void
			name(const char* rhs) noexcept {
				_name = rhs;
			}

			constexpr explicit
			operator bool() const noexcept {
				return this->_id != 0;
			}

			constexpr bool
			operator !() const noexcept {
				return this->_id == 0;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Type& rhs) {
				return out << rhs.name() << '(' << rhs.id() << ')';
			}

			static
			void write_object(kernel_type& kernel, sysx::packetstream& out) {
				const Type type = kernel.type();
				if (!type) {
					std::stringstream msg;
					msg << "Can not find type for kernel id=" << kernel.id();
					throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				out << type.id();
				kernel.write(out);
			}

			template<class Func>
			static void
			read_object(Types<Type>& types, sysx::packetstream& packet, Func callback) {
				id_type id;
				packet >> id;
				const Type* type = types.lookup(id);
				if (type == nullptr) {
					std::stringstream msg;
					msg << "Demarshalling of non-kernel object with typeid = " << id << " was prevented.";
					throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				kernel_type* kernel = nullptr;
				try {
					kernel = type->read(packet);
				} catch (std::bad_alloc& err) {
					std::stringstream msg;
					msg << "Allocation error. Demarshalled kernel was prevented"
						" from allocating too much memory. " << err.what();
					throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				callback(kernel);
			}
	
			id_type _id;
			std::string _name;
//			construct_type construct;
			read_type read;
		};

//		template<class Sub, class T, class Base=T>
//		struct Type_init: public Base {
//
//			constexpr const Type<T>
//			type() const noexcept override {
//				return _type;
//			}
//
//			void
//			write(sysx::packetstream& out) override {
//				Base::write(out);
//				write_impl(out);
//			}
//
//			void
//			read(sysx::packetstream& in) override {
//				Base::read(in);
//				read_impl(in);
//			}
//
//			virtual void
//			write_impl(sysx::packetstream& out) = 0;
//
//			virtual void
//			read_impl(sysx::packetstream& in) = 0;
//
//		private:
//			struct Init: public Type<T> {
//				Init() {
//					this->read = [] (sysx::packetstream& in) {
//						Sub* k = new Sub;
//						k->read(in);
//						return k;
//					};
//
//					Sub::init_type(this);
//					Type<T>::types().register_type(this);
//				}
//			};
//
//			// Static template members are initialised on demand,
//			// i.e. only if they are accessed in a program.
//			// This function tries to fool the compiler.
//			virtual typename Type<T>::id_type
//			unused() { return _type.id(); }
//
//			static const Init _type;
//		};
//
//		template<class Sub, class T, class Base>
//		const typename Type_init<Sub, T, Base>::Init Type_init<Sub, T, Base>::_type;

		template<class T>
		struct Instances {

			T* lookup(Id id) {
				std::unique_lock<std::mutex> lock(_mutex);
				auto result = _instances.find(id);
				return result == _instances.end() ? nullptr : result->second; 
			}

			void register_instance(T* inst) {
				std::unique_lock<std::mutex> lock(_mutex);
				_instances[inst->id()] = inst;
			}

			void free_instance(T* inst) {
				std::unique_lock<std::mutex> lock(_mutex);
				_instances.erase(inst->id());
			}
		
			friend std::ostream& operator<<(std::ostream& out, const Instances& rhs) {
				// TODO: this function is not thread-safe
				std::ostream_iterator<Entry> it(out, "\n");
				std::copy(rhs._instances.cbegin(), rhs._instances.cend(), it);
				return out;
			}

		private:

			struct Entry {
				Entry(const std::pair<const Id, T*>& k): _kernel(k.second) {}

				friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
					return out
						<< "/instance/"
						<< rhs._kernel->id()
						<< " = "
						<< rhs.safe_type_name();
				}

			private:

				const char* safe_type_name() const {
					return !_kernel->type()
						? "_identifiable"
						: _kernel->type().name();
				}

				T* _kernel;
			};

			std::unordered_map<Id, T*> _instances;
			std::mutex _mutex;
		};
	
	}

	struct Identifiable_tag {};
	using components::Type;

}

#endif // FACTORY_TYPE_HH
