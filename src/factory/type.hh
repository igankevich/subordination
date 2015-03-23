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
	
			Type(): _name() {}
	
			Type(const Type& rhs):
				_id(rhs._id),
				_name(rhs._name),
				construct(rhs.construct),
				read_object(rhs.read_object),
				write_object(rhs.write_object),
				read_and_send(rhs.read_and_send)
			{}

			Type_id id() const { return _id; }
			void id(Type_id i) { _id = i; }

			const char* name() const { return _name.c_str(); }
			void name(const char* n) { _name = n; }

			std::function<K* ()> construct;
			std::function<void (Foreign_stream& in, K* rhs)> read_object;
			std::function<void (Foreign_stream& out, K* rhs)> write_object;
			std::function<void (Foreign_stream& in, Callback callback)> read_and_send;
	
			friend std::ostream& operator<<(std::ostream& out, const Type<K>& rhs) {
				return out << rhs.name() << '(' << rhs.id() << ')';
			}

			class Types {
			public:
	
				typedef Type<K> T;
	
				const T* lookup(Type_id type_name) const {
					auto result = _types_by_id.find(type_name);
					return result == _types_by_id.end() ? nullptr : result->second; 
				}
		
				K* read_object(Foreign_stream& packet) const {
					K* object = nullptr;
					Type_id id;
					packet >> id;
					const T* type = lookup(id);
					if (type == nullptr) {
						std::stringstream msg;
						msg << "Demarshalling of non-kernel object with typeid = " << id << " was prevented.";
						throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
					}
					try {
						object = type->construct();
						type->read_object(packet, object);
					} catch (std::bad_alloc& err) {
						std::stringstream msg;
						msg << "Allocation error. Demarshalled kernel was prevented"
							" from allocating too much memory. " << err.what();
						throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
					}
					return object;
				}

				void read_and_send_object(Foreign_stream& packet, typename T::Callback callback) const {
					Type_id id;
					packet >> id;
					const T* type = lookup(id);
					if (type == nullptr) {
						std::stringstream msg;
						msg << "Demarshalling of non-kernel object with typeid = " << id << " was prevented.";
						throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
					}
					try {
						type->read_and_send(packet, callback);
					} catch (std::bad_alloc& err) {
						std::stringstream msg;
						msg << "Allocation error. Demarshalled kernel was prevented"
							" from allocating too much memory. " << err.what();
						throw Marshalling_error(msg.str(), __FILE__, __LINE__, __func__);
					}
				}
	
				void register_type(T* type) {
					const T* existing_type = lookup(type->id());
					if (existing_type != nullptr) {
						std::stringstream msg;
						msg << "'" << *type << "' and '" << *existing_type
							<< "' have the same type identifiers.";
						throw Error(msg.str(), __FILE__, __LINE__, __func__);
					}
					_types_by_id[type->id()] = type;
				}
		
				friend std::ostream& operator<<(std::ostream& out, const Types& rhs) {
					std::ostream_iterator<Entry> it(out, "\n");
					std::copy(rhs._types_by_id.cbegin(), rhs._types_by_id.cend(), it);
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

				std::unordered_map<Type_id, T*> _types_by_id;
			};

			class Instances {

			public:

				Instances(): _instances() {}

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
		};

		template<class Sub, class Type, class K, class Base=K>
		class Type_init: public Base {
		public:
			const Type* type() const { return &_type; }

		private:
			struct Init: public Type {
				Init() {
					this->construct = [] { return new Sub; };
					this->read_object = [] (Foreign_stream& in, K* rhs) { rhs->read(in); };
					this->write_object = [] (Foreign_stream& out, K* rhs) { rhs->write(out); };
					this->read_and_send = [] (Foreign_stream& in, typename Type::Callback callback) {
						Sub* k = new Sub;
						k->read(in);
						callback(k);
						if (k->principal()) {
							K* p = Type::instances().lookup(k->principal()->id());
							if (p == nullptr) {
								k->result(Result::NO_PRINCIPAL_FOUND);
								throw No_principal_found<K>(k);
							}
							k->principal(p);
						}
						factory_send(k);
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
			virtual Id unused() { return _type.id(); }

			static const Init _type;
		};

		template<class Sub, class Type, class K, class Base>
		const typename Type_init<Sub, Type, K, Base>::Init Type_init<Sub, Type, K, Base>::_type;

		Id factory_start_id() {
			const char* id = ::getenv("START_ID");
			Id i = 1000;
			if (id != NULL) {
				std::stringstream tmp;
				tmp << id;
				if (!(tmp >> i) || i == ROOT_ID) {
					i = 1000;
					std::clog << "Bad START_ID value: " << id << std::endl;
				}
			}
			std::clog << "START_ID = " << i << std::endl;
			return i;
		}

		Id factory_generate_id() {
			static std::atomic<Id> counter(factory_start_id());
			return counter++;
		}

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

		};
	
	}

}
