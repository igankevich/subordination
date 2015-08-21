namespace factory {

	typedef uint32_t Id;

	const Id ROOT_ID = 0;

//	std::string demangle_type_name(const char* type_name) {
//		int status = 0;
//		char* real_name = abi::__cxa_demangle(type_name, 0, 0, &status);
//		std::string str = status == 0 ? real_name : type_name;
//		delete[] real_name;
//		return str;
//	}
//
//	template<class X>
//	std::string demangle_type_name(const X& x) {
//		return demangle_type_name(typeid(x).name());
//	}

	namespace components {

		template<class K>
		class Type {
		public:

			/// A portable type id
			typedef int16_t Type_id;
			typedef std::function<void(K*)> Callback;
			typedef K kernel_type;
			typedef Type<K> this_type;

			typedef stdx::log<Type> this_log;
	
			constexpr Type():
				_id(0),
				_name(),
				construct(),
				read_and_send()
			{}
	
			constexpr Type(const Type& rhs):
				_id(rhs._id),
				_name(rhs._name),
				construct(rhs.construct),
				read_and_send(rhs.read_and_send)
			{}

			constexpr Type_id id() const { return _id; }
			void id(Type_id i) { _id = i; }

			constexpr const char* name() const { return _name.c_str(); }
			void name(const char* n) { _name = n; }

			constexpr explicit operator bool() const { return this->_id != 0; }
			constexpr bool operator !() const { return this->_id == 0; }

			friend std::ostream& operator<<(std::ostream& out, const Type<K>& rhs) {
				return out << rhs.name() << '(' << rhs.id() << ')';
			}

			static
			void write_object(kernel_type& kernel, packstream& out) {
				const this_type* type = kernel.type();
				if (type == nullptr) {
					std::stringstream msg;
					msg << "Can not find type for kernel id=" << kernel.id();
					throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				out << type->id();
				this_log() << "Type::write_object: kernel=" << kernel << std::endl;
				kernel.write(out);
			}

			static
			void read_object(packstream& packet,
				Callback after_read, Callback send_object)
			{
				Type_id id;
				packet >> id;
				if (!packet) return;
				const this_type* type = this_type::types().lookup(id);
				if (type == nullptr) {
					std::stringstream msg;
					msg << "Demarshalling of non-kernel object with typeid = " << id << " was prevented.";
					throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
				}
				try {
					type->read_and_send(packet, after_read, send_object);
				} catch (std::bad_alloc& err) {
					std::stringstream msg;
					msg << "Allocation error. Demarshalled kernel was prevented"
						" from allocating too much memory. " << err.what();
					throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
				}
			}
	

			class Types {
			public:
	
				typedef Type<K> T;

				Types() = default;
	
				const T* lookup(Type_id type_name) const {
					auto result = this->_types.find(type_name);
					return result == this->_types.end() ? nullptr : result->second; 
				}

				void register_type(T* type) {
					const T* existing_type = this->lookup(type->id());
					if (existing_type != nullptr) {
						std::stringstream msg;
						msg << "'" << *type << "' and '" << *existing_type
							<< "' have the same type identifiers.";
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}
					this->_types[type->id()] = type;
				}
		
				friend std::ostream& operator<<(std::ostream& out, const Types& rhs) {
					std::ostream_iterator<Entry> it(out, "\n");
					std::copy(rhs._types.cbegin(), rhs._types.cend(), it);
					return out;
				}
		
			private:

				struct Entry {
					Entry(const std::pair<const Type_id, T*>& k): _type(k.second) {}

					friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
						return out
							<< "/type/"
							<< rhs._type->id()
							<< " = "
							<< *rhs._type;
					}

				private:
					T* _type;
				};

				std::unordered_map<Type_id, T*> _types;
			};

			class Instances {

			public:

				Instances(): _instances(), _mutex() {}

				K* lookup(Id id) {
					std::unique_lock<std::mutex> lock(_mutex);
					auto result = _instances.find(id);
					return result == _instances.end() ? nullptr : result->second; 
				}

				void register_instance(K* inst) {
					std::unique_lock<std::mutex> lock(_mutex);
					_instances[inst->id()] = inst;
				}

				void free_instance(K* inst) {
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
					Entry(const std::pair<const Id, K*>& k): _kernel(k.second) {}

					friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
						return out
							<< "/instance/"
							<< rhs._kernel->id()
							<< " = "
							<< rhs.safe_type_name();
					}

				private:

					const char* safe_type_name() const {
						return _kernel->type() == nullptr
							? "_identifiable"
							: _kernel->type()->name();
					}

					K* _kernel;
				};

				std::unordered_map<Id, K*> _instances;
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

		private:

			Type_id _id;
			std::string _name;

		protected:
			std::function<K* ()> construct;
			std::function<void (packstream& in, Callback callback, Callback)> read_and_send;
		};

		template<class Sub, class Type, class K, class Base=K>
		class Type_init: public Base {
		public:
			constexpr const Type* type() const { return &_type; }

		private:
			struct Init: public Type {
				using typename Type::Callback;
				Init() {
					this->construct = [] { return new Sub; };
					this->read_and_send = [] (packstream& in, Callback callback, Callback call2) {
						Sub* k = new Sub;
						k->read(in);
//						if (in) {
							//TODO: always true
							// TODO: there should be only one callback
							callback(k);
							if (k->principal()) {
								K* p = Type::instances().lookup(k->principal()->id());
								if (p == nullptr) {
									k->result(Result::NO_PRINCIPAL_FOUND);
									throw No_principal_found<K>(k);
								}
								k->principal(p);
							}
							call2(k);
//						}
					};

//					this->type_id(typeid(Sub));
					Sub::init_type(this);
					try {
						Type::types().register_type(this);
					} catch (std::exception& err) {
						std::clog << "Error during initialisation of types. " << err.what() << std::endl;
						std::abort();
					}
				}
			};

			// Static template members are initialised on demand,
			// i.e. only if they are accessed in a program.
			// This function tries to fool the compiler.
			virtual typename Type::Type_id unused() { return _type.id(); }

			static const Init _type;
		};

		template<class Sub, class Type, class K, class Base>
		const typename Type_init<Sub, Type, K, Base>::Init Type_init<Sub, Type, K, Base>::_type;

		Id factory_start_id();
		Id factory_generate_id();

		template<class K, class Type>
		class Identifiable: public K {

		public:

			explicit Identifiable(Id i, bool b=true) {
				this->id(i);
				if (b) {
					Type::instances().register_instance(this);
				}
			}

			Identifiable() {
				this->id(factory_generate_id());
				Type::instances().register_instance(this);
			}
			// TODO: call free_instance() in destructor ???

		};
	
	}

}
