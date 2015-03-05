namespace factory {

	enum struct Result: uint32_t {
		SUCCESS = 0,
		ENDPOINT_NOT_CONNECTED = 1,
		NO_UPSTREAM_SERVERS_LEFT = 2,
		UNDEFINED = 3
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

			bool moves_upstream() const { return _result == Result::UNDEFINED; }
			bool moves_downstream() const { return _result != Result::UNDEFINED; }

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

		// TODO: make this class an aspect of a Kernel rather than a separate entity
		template<class A>
		struct Principal: public A {

			typedef Principal<A> This;

			Principal(): _principal(nullptr) {}

			const A* principal() const { return _principal; }
			void principal(A* rhs) { _principal = rhs; }

			void run_act() {
				switch (this->result()) {
					case Result::UNDEFINED:
						this->act();
						this->result(Result::SUCCESS);
						break;
					default:
						if (principal() == nullptr) {
							if (this->parent() == principal()) {
								delete this;
								factory_log(Level::KERNEL) << "SHUTDOWN" << std::endl;
								factory_stop();
							}
						} else {
							bool del = *principal() == *this->parent();
							if (this->result() == Result::SUCCESS) {
								_principal->react(this);
							} else {
								_principal->error(this);
							}
							if (del) {
								delete this;
							}
						}
				}
			}

			void read(Foreign_stream& in) {
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
				_principal = Type<A>::instances().lookup(principal_id);
				if (_principal == nullptr) {
					std::stringstream str;
					str << "Can not find principal kernel on this server, kernel id = " << principal_id;
					throw Durability_error(str.str(), __FILE__, __LINE__, __func__);
				}
			}

			void write(Foreign_stream& out) {
				factory_log(Level::KERNEL) << "WRITING PRINCIPAL = " << principal()->id() << std::endl;
				out << _principal->id();
			}
		
			virtual const Type<Principal<A>>* type() const { return nullptr; }

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
			inline void downstream(S* this_server, This* a, A* b) {
				a->principal(b);
				this_server->send(a);
			}

			template<class S>
			inline void commit(S* this_server) {
				downstream(this_server, this, this->parent());
			}

		private:
			A* _principal;
		};

		template<class A>
		class Reflecting_kernel: public Kernel_link<Reflecting_kernel<A>, A> {
		public:
			typedef Transient_kernel<Reflecting_kernel<A>> Transient;

			Reflecting_kernel(): _parent(nullptr), _delete_parent(false) {}

			~Reflecting_kernel() {
				if (_parent != nullptr && _delete_parent) {
					delete _parent;
				}
			}
			virtual void react(Reflecting_kernel<A>*) {
//				factory_log(Level::KERNEL) << "Empty react in " << demangle_type_name(*this) << std::endl;
			}
			virtual void error(Reflecting_kernel<A>* rhs) {
				react(rhs);
			}

//			template<class S>
//			inline void react_to(Reflecting_kernel<A>* par, S* this_server) {
//				if (par == nullptr) {
//					this_server->stop();
//				} else {
//					par->react(this);
//				}
//				if (_parent == par) {
//					delete this;
//				}
//			}
			const Reflecting_kernel<A>* parent() const { return _parent; }
			Reflecting_kernel<A>* parent() { return _parent; }
			void parent(Reflecting_kernel<A>* p) { _parent = p; }

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
					_delete_parent = true;
				}
			}

			void write_impl(Foreign_stream& out) {
				factory_log(Level::KERNEL) << "WRITING PARENT " << parent()->id() << std::endl;
				out << (parent() == nullptr ? ROOT_ID : parent()->id());
			}

		
		private:
			Reflecting_kernel<A>* _parent;
			bool _delete_parent;
		};

		template<class A>
		struct Service: public A {
			virtual void stop() = 0;
//			virtual void wait() = 0;
		};


		template<class K>
		struct Mobile: public K {

			virtual void read(Foreign_stream& ) { 
//				Id i;
//				in >> i;
//				id(i);
//				Endpoint fr;
//				in >> fr;
//				from(fr);
			}

			virtual void write(Foreign_stream& ) {
//				out << id();
//				out << from();
			}

			virtual void read_impl(Foreign_stream&) {}
			virtual void write_impl(Foreign_stream&) {}

			virtual Id id() const { return ROOT_ID; }
			virtual void id(Id) {}

			virtual Resource resource() const { return ""; }
			virtual Endpoint from() const { return Endpoint(); }
			virtual void from(Endpoint) {}

			bool operator==(const Mobile<K>& rhs) const {
				return this == &rhs || (id() != ROOT_ID && rhs.id() != ROOT_ID && id() == rhs.id());
			}

		};

	}

}
