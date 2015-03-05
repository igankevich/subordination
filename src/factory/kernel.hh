namespace factory {

	enum struct Result: uint16_t {
		SUCCESS = 0,
		UNDEFINED = 1,
		UNDEFINED_DOWNSTREAM = 2,
		ENDPOINT_NOT_CONNECTED = 3,
		NO_UPSTREAM_SERVERS_LEFT = 4,
		NO_PRINCIPAL_FOUND = 5
	};

	template<class K>
	struct No_principal_found {
		explicit No_principal_found(K* k): _kernel(k) {}
		K* kernel() { return _kernel; }
	private:
		K* _kernel;
	};

	namespace components {

		struct Basic_kernel {

			Basic_kernel(): _result(Result::UNDEFINED) {}
			virtual ~Basic_kernel() {}

			virtual bool is_profiled() const { return true; }
			virtual bool is_transient() const { return false; }
			virtual void act() {}

			Result result() const { return _result; }
			void result(Result rhs) { _result = rhs; }

		private:
			Result _result;
		};

		// No one lives forever.
		template<class K>
		struct Transient_kernel: public Identifiable<K, Type<K>> {
			explicit Transient_kernel(Id i): Identifiable<K, Type<K>>(i, false) {}
			bool is_transient() const { return true; }
		};

		// The class ensures that certain methods (e.g. read, write)
		// are called in a sequence starting from method in the top superclass
		// and ending with method in the bottom subclass (like constructors and destructors).
		// In contrast to simply overriding method
		// this approach takes care of all superclasses' members being properly marshalled and demarshalled
		// from the data stream thus ensuring that no method's top implementations are omitted.
		template<class Sub, class Super>
		struct Kernel_link: public Super {

			void read(Foreign_stream& in) {
				Super::read(in);
				static_cast<Sub*>(this)->Sub::read_impl(in);
			}

			void write(Foreign_stream& out) {
				Super::write(out);
				static_cast<Sub*>(this)->Sub::write_impl(out);
			}

		};

		template<class A>
		struct Principal: public Kernel_link<Principal<A>, A> {

			typedef Principal<A> This;
			typedef Transient_kernel<This> Transient;

			Principal(): _parent(nullptr), _principal(nullptr) {}

			~Principal() {
				del(_parent);
				del(_principal);
			}

			const This* principal() const { return _principal; }
			This* principal() { return _principal; }
			void principal(This* rhs) {
				_principal = rhs;
				if (this->result() == Result::UNDEFINED) {
					this->result(Result::UNDEFINED_DOWNSTREAM);
				}
			}

			const This* parent() const { return _parent; }
			This* parent() { return _parent; }
			void parent(This* p) { _parent = p; }

			bool moves_upstream() const { return this->result() == Result::UNDEFINED && principal() == nullptr; }
			bool moves_downstream() const {
				return this->result() != Result::UNDEFINED
					|| this->result() == Result::UNDEFINED_DOWNSTREAM
					|| principal() != nullptr; }

			void read_impl(Foreign_stream& in) {
				if (_parent != nullptr) {
					std::stringstream s;
					s << "Parent is not null while reading from the data stream. Parent=";
					s << _parent;
					throw Error(s.str(), __FILE__, __LINE__, __func__);
				}
				Id parent_id;
				in >> parent_id;
				factory_log(Level::KERNEL) << "READING PARENT " << parent_id << std::endl;
				if (parent_id != ROOT_ID) {
					_parent = new Transient(parent_id);
				}
				if (this->moves_downstream()) {
					if (_principal != nullptr) {
						throw Error("Principal kernel is not null while reading from the data stream.",
							__FILE__, __LINE__, __func__);
					}
					Id principal_id;
					in >> principal_id;
					factory_log(Level::KERNEL) << "READING PRINCIPAL " << principal_id << std::endl;
					// TODO: move this code to server and create instance repository in each server.
					if (principal_id == ROOT_ID) {
						throw Error("Principal of a mobile kernel can not be null.",
							__FILE__, __LINE__, __func__);
					}
					_principal = Type<This>::instances().lookup(principal_id);
					if (_principal == nullptr) {
						this->result(Result::NO_PRINCIPAL_FOUND);
						throw No_principal_found<This>(this);
//						std::stringstream str;
//						str << "Can not find principal kernel on this server, kernel id = "
//							<< principal_id
//							<< ", result = " << uint16_t(this->result());
//						throw Durability_error(str.str(), __FILE__, __LINE__, __func__);
					}
				}
			}

			void write_impl(Foreign_stream& out) {
				factory_log(Level::KERNEL) << "WRITING PARENT " << parent()->id() << std::endl;
				out << (parent() == nullptr ? ROOT_ID : parent()->id());
				if (this->moves_downstream()) {
					if (principal() == nullptr) {
						throw Durability_error("Principal is null while writing a kernel to a stream.",
							__FILE__, __LINE__, __func__);
					}
					Id id = principal()->id();
					factory_log(Level::KERNEL) << "WRITING PRINCIPAL = " << id << std::endl;
					out << id;
				}
			}

			virtual void react(This*) {
				factory_log(Level::KERNEL) << "Empty react in " << std::endl;
			}

			virtual void error(This* rhs) { react(rhs); }

			virtual const Type<This>* type() const { return nullptr; }

			void run_act() {
				switch (this->result()) {
					case Result::UNDEFINED:
						this->act();
						this->result(Result::SUCCESS);
						break;
					default:
						factory_log(Level::KERNEL) << "Result is defined" << std::endl;
						if (this->principal() == nullptr) {
							factory_log(Level::KERNEL) << "Principal is null" << std::endl;
							if (this->parent() == this->principal()) {
								delete this;
								factory_log(Level::KERNEL) << "SHUTDOWN" << std::endl;
								factory_stop();
							}
						} else {
							factory_log(Level::KERNEL) << "Principal is not null" << std::endl;
							bool del = *this->principal() == *this->parent();
							if (this->result() == Result::SUCCESS) {
								factory_log(Level::KERNEL) << "Principal react" << std::endl;
								this->principal()->react(this);
							} else {
								this->principal()->error(this);
							}
							if (del) {
								delete this;
							}
						}
				}
			}
		
		public:
			template<class S>
			inline void upstream(S* this_server, This* a) {
				a->parent(this);
				this_server->send(a);
			}

			template<class S>
			inline void downstream(S* this_server, This* a) {
				a->parent(this);
				a->principal(this);
				this_server->send(a);
			}

			template<class S>
			inline void downstream(S* this_server, This* a, This* b) {
				a->principal(b);
				this->result(Result::SUCCESS);
				this_server->send(a);
			}

			template<class S>
			inline void commit(S* this_server) {
				downstream(this_server, this, this->parent());
			}

		private:

			void del(This* ref) {
				if (ref != nullptr && ref->is_transient()) {
					delete ref;
				}
			}

			This* _parent;
			This* _principal;
		};

		template<class A>
		struct Service: public A {
			virtual void stop() = 0;
//			virtual void wait() = 0;
		};


		template<class K>
		struct Mobile: public K {

			virtual void read(Foreign_stream& in) { 
				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				uint16_t r;
				in >> r;
				this->result(Result(r));
				factory_log(Level::KERNEL) << "Reading result = " << r << std::endl;
//				Id i;
//				in >> i;
//				id(i);
//				Endpoint fr;
//				in >> fr;
//				from(fr);
			}

			virtual void write(Foreign_stream& out) {
				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				uint16_t r = static_cast<uint16_t>(this->result());
				factory_log(Level::KERNEL) << "Writing result = " << r << std::endl;
				out << r;
//				out << id();
//				out << from();
			}

			virtual void read_impl(Foreign_stream&) {}
			virtual void write_impl(Foreign_stream&) {}

			virtual Id id() const { return ROOT_ID; }
			virtual void id(Id) {}

			virtual Endpoint from() const { return Endpoint(); }
			virtual void from(Endpoint) {}

			bool operator==(const Mobile<K>& rhs) const {
				return this == &rhs || (id() != ROOT_ID && rhs.id() != ROOT_ID && id() == rhs.id());
			}

		};

	}

}
