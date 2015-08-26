namespace factory {

	typedef uint32_t Id;

	const Id ROOT_ID = 0;

	namespace components {

		template<class T>
		struct Type {

			/// A portable type id
			typedef int16_t id_type;
			typedef T kernel_type;
//			typedef std::function<T* ()> construct_type;
			typedef std::function<T* (packstream&)> read_type;

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
			void write_object(kernel_type& kernel, packstream& out) {
				const Type type = kernel.type();
				if (!type) {
					std::stringstream msg;
					msg << "Can not find type for kernel id=" << kernel.id();
					throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				out << type.id();
				this_log() << "Type::write_object: kernel=" << kernel << std::endl;
				kernel.write(out);
			}

			template<class Func>
			static void
			read_object(packstream& packet, Func callback) {
				id_type id;
				packet >> id;
				if (!packet) return;
				const Type* type = Type::types().lookup(id);
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
	

			struct Types {
	
				const Type*
				lookup(id_type type_name) const noexcept {
					auto result = this->_types.find(type_name);
					return result == this->_types.end() ? nullptr : result->second; 
				}

				void
				register_type(Type* type) {
					const Type* existing_type = this->lookup(type->id());
					if (existing_type != nullptr) {
						std::stringstream msg;
						msg << "'" << *type << "' and '" << *existing_type
							<< "' have the same type identifiers.";
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}
					this->_types[type->id()] = type;
				}
		
				friend std::ostream&
				operator<<(std::ostream& out, const Types& rhs) {
					std::ostream_iterator<Entry> it(out, "\n");
					std::copy(rhs._types.cbegin(), rhs._types.cend(), it);
					return out;
				}
		
			private:

				struct Entry {
					Entry(const std::pair<const id_type, Type*>& k): _type(k.second) {}

					friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
						return out
							<< "/type/"
							<< rhs._type->id()
							<< " = "
							<< *rhs._type;
					}

				private:
					Type* _type;
				};

				std::unordered_map<id_type, Type*> _types;
			};

			class Instances {

			public:

				Instances(): _instances(), _mutex() {}

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

			static Types& types() {
				static Types x;
				return x;
			}
			
			static Instances& instances() {
				static Instances x;
				return x;
			}


			id_type _id;
			std::string _name;
//			construct_type construct;
			read_type read;
		};

		template<class Sub, class T, class Base=T>
		struct Type_init: public Base {

			constexpr const Type<T>
			type() const noexcept override {
				return _type;
			}

			void
			write(packstream& out) override {
				Base::write(out);
				write_impl(out);
			}

			void
			read(packstream& in) override {
				Base::read(in);
				read_impl(in);
			}

			virtual void
			write_impl(packstream& out) = 0;

			virtual void
			read_impl(packstream& in) = 0;

		private:
			struct Init: public Type<T> {
				Init() {
					this->read = [] (packstream& in) {
						Sub* k = new Sub;
						k->read(in);
						return k;
					};

					Sub::init_type(this);
					Type<T>::types().register_type(this);
				}
			};

			// Static template members are initialised on demand,
			// i.e. only if they are accessed in a program.
			// This function tries to fool the compiler.
			virtual typename Type<T>::id_type
			unused() { return _type.id(); }

			static const Init _type;
		};

		template<class Sub, class T, class Base>
		const typename Type_init<Sub, T, Base>::Init Type_init<Sub, T, Base>::_type;

		// TODO: deprecated
		inline Id
		factory_start_id() noexcept {

			struct factory_start_id_t{};
			typedef stdx::log<factory_start_id_t> this_log;

			constexpr static const Id
			DEFAULT_START_ID = 1000;

			Id i = unix::this_process::getenv("START_ID", DEFAULT_START_ID);
			if (i == ROOT_ID) {
				i = DEFAULT_START_ID;
				this_log() << "Bad START_ID value: " << ROOT_ID << std::endl;
			}
			this_log() << "START_ID = " << i << std::endl;
			return i;
		}

		// TODO: deprecated
		inline Id
		factory_generate_id() {
			static std::atomic<Id> counter(factory_start_id());
			return counter++;
		}

		template<class T>
		struct Identifiable: public T {

			explicit
			Identifiable(Id i, bool b=true) {
				this->id(i);
				if (b) {
					Type<T>::instances().register_instance(this);
				}
			}

			Identifiable() {
				this->id(factory_generate_id());
				Type<T>::instances().register_instance(this);
			}
			// TODO: call free_instance() in destructor ???

		};
	
	}

}
