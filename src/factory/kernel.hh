namespace factory {

	namespace components {

		struct Kernel_base {
			Kernel_base() {}
			virtual ~Kernel_base() {}
			virtual bool is_profiled() const { return true; }
			virtual bool is_transient() const { return false; }
			virtual void act() {}
		};

		// No one lives forever.
		template<class K>
		struct Transient_kernel: public Identifiable<K, Type<K>> {
			explicit Transient_kernel(Id i): Identifiable<K, Type<K>>(i) {}
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
		struct Kernel_pair: public Type_init<Kernel_pair<A>, Type<A>, A> {

			Kernel_pair(): _subordinate(nullptr), _principal(nullptr) {}

			Kernel_pair(A* subordinate, A* principal):
				_subordinate(subordinate), _principal(principal) {}

			const A* principal() const { return _principal; }
			const A* subordinate() const { return _subordinate; }

			void act() {
				if (_principal == nullptr) {
					if (_subordinate->parent() == _principal) {
						delete _subordinate;
					}
				} else {
					bool del = *_principal == *_subordinate->parent();
					_principal->react(_subordinate);
					if (del) {
						delete _subordinate;
					}
				}
				if (_principal == nullptr) {
					delete this;
					factory_stop();
					std::clog << "SHUTDOWN" << std::endl;
				} else {
					delete this;
				}
			}

			void read(Foreign_stream& in) {
				if (_principal != nullptr) {
					throw Error("Principal kernel is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				if (_subordinate != nullptr) {
					throw Error("Subordinate kernel is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				Id principal_id;
				in >> principal_id;
				std::clog << "READING PRINCIPAL " << principal_id << std::endl;
				if (principal_id != ROOT_ID) {
					_principal = Type<A>::instances().lookup(principal_id);
					if (_principal == nullptr) {
						std::stringstream str;
						str << "Can not find principal kernel on this server, kernel id = " << principal_id;
						throw Durability_error(str.str(), __FILE__, __LINE__, __func__);
					}
				}
				_subordinate = Type<A>::types().read_object(in);
			}

			void write(Foreign_stream& out) {
				std::clog << "WRITING PRINCIPAL = " << principal()->id() << std::endl;
				out << _principal->id();
				out << _subordinate->type()->id();
				_subordinate->write(out);
			}

			static void init_type(Type<A>* type) {
				type->id(0);
				type->name("_Kernel_pair");
			}

		private:
			A* _subordinate;
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
//				std::clog << "Empty react in " << demangle_type_name(*this) << std::endl;
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
					throw Error("Parent is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				Id parent_id;
				in >> parent_id;
				std::clog << "READING PARENT " << parent_id << std::endl;
				if (parent_id != ROOT_ID) {
					_parent = new Transient(parent_id);
					_delete_parent = true;
				}
			}

			void write_impl(Foreign_stream& out) {
				std::clog << "WRITING PARENT " << parent()->id() << std::endl;
				out << (parent() == nullptr ? ROOT_ID : parent()->id());
			}

			virtual const Type<Reflecting_kernel<A>>* type() const { return nullptr; }
		
		public:
			template<class S>
			inline void upstream(S* this_server, Reflecting_kernel<A>* a) {
				a->_parent = this; this_server->send(a);
			}

			template<class S>
			inline void downstream(S* this_server, Reflecting_kernel<A>* a) {
				a->_parent = this;
				this_server->send(new Kernel_pair<Reflecting_kernel<A>>(a, this));
			}

			template<class S>
			inline void downstream(S* this_server, Reflecting_kernel<A>* a, Reflecting_kernel<A>* b) {
//				this_server->send(make_pair(a, b));
				this_server->send(new Kernel_pair<Reflecting_kernel<A>>(a, b));
			}

			template<class S>
			inline void commit(S* this_server) {
				downstream(this_server, this, this->_parent);
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

			virtual void read(Foreign_stream& in) { 
				Id i;
				in >> i;
				id(i);
//				Endpoint fr;
//				in >> fr;
//				from(fr);
			}

			virtual void write(Foreign_stream& out) {
				out << id();
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
