#ifndef FACTORY_CONFIGURATION
namespace factory {

	namespace components {

		struct Basic_kernel {

			typedef std::chrono::steady_clock Clock;
			typedef Clock::time_point Time_point;
			typedef Clock::duration Duration;
			typedef std::bitset<1> Flags;
			
			enum struct Flag {
				DELETED = 0
			};

			Basic_kernel() {}
			virtual ~Basic_kernel() {}

			virtual void act() {}

			Result result() const { return _result; }
			void result(Result rhs) { _result = rhs; }

			// timed kernels
			Time_point at() const { return _at; }
			void at(Time_point t)  { _at = t; }
			void after(Duration delay) { _at = Clock::now() + delay; }
			bool timed() const { return _at != Time_point(Duration::zero()); }

			// flags
			Flags flags() const { return _flags; }
			void setf(Flag f) { _flags.set(static_cast<size_t>(f)); }
			void unsetf(Flag f) { _flags.reset(static_cast<size_t>(f)); }
			bool isset(Flag f) const { return _flags.test(static_cast<size_t>(f)); }

		private:
			Result _result = Result::UNDEFINED;
			Time_point _at = Time_point(Duration::zero());
			Flags _flags = 0;
		};

		template<class K>
		struct Kernel_ref {

			constexpr Kernel_ref(): _kernel(nullptr), _temp(false) {}
			constexpr Kernel_ref(K* rhs): _kernel(rhs), _temp(false) {}
			Kernel_ref(Id rhs):
				_kernel(rhs == ROOT_ID ? nullptr : new Transient_kernel(rhs)),
				_temp(rhs != ROOT_ID) {}
			Kernel_ref(const Kernel_ref& rhs): _kernel(rhs._kernel), _temp(rhs._temp) {
				if (_temp) {
					Id i = _kernel->id();
					_kernel = new Transient_kernel(i);
				}
			}
			~Kernel_ref() {
				if (_temp) delete _kernel;
			}

			// dereference operators
			K* operator->() { return _kernel; }
			const K* operator->() const { return _kernel; }
			K& operator*() { return *_kernel; }
			const K& operator*() const { return *_kernel; }

			Kernel_ref& operator=(const Kernel_ref& rhs) {
				if (&rhs == this) return *this;
				if (_temp) {
					delete _kernel;
				}
				_temp = rhs._temp;
				if (_temp) {
					Id i = rhs._kernel->id();
					_kernel = new Transient_kernel(i);
				} else {
					_kernel = rhs._kernel;
				}
				return *this;
			}

			Kernel_ref& operator=(K* rhs) {
				if (rhs == _kernel) return *this;
				if (_temp) {
					delete _kernel;
					_temp = false;
				}
				_kernel = rhs;
				return *this;
			}

			Kernel_ref& operator=(Id rhs) {
				if (_temp) {
					delete _kernel;
				}
				if (rhs == ROOT_ID) {
					_kernel = nullptr;
					_temp = false;
				} else {
					_kernel = new Transient_kernel(rhs);
					_temp = true;
				}
				return *this;
			}

			K* ptr() { return _kernel; }
			const K* ptr() const { return _kernel; }

			constexpr explicit operator bool() const { return _kernel != nullptr; }
			constexpr bool operator !() const { return _kernel == nullptr; }

			friend std::ostream& operator<<(std::ostream& out, const Kernel_ref<K>& rhs) {
				return out << rhs->id();
			}

		private:
			K* _kernel;
			bool _temp;

			// No one lives forever.
			struct Transient_kernel: public Identifiable<K, Type<K>> {
				explicit Transient_kernel(Id i): Identifiable<K, Type<K>>(i, false) {}
			};
		};


		// The class ensures that certain methods (e.g. read, write)
		// are called in a sequence starting from method in the top superclass
		// and ending with method in the bottom subclass (like constructors and destructors).
		// In contrast to simply overriding method
		// this approach takes care of all superclasses' members being properly marshalled and demarshalled
		// from the data stream thus ensuring that no method's top implementations are omitted.
		template<class Sub, class Super>
		struct Kernel_link: public Super {

			virtual ~Kernel_link() {}

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
			typedef Kernel_ref<This> Ref;
			typedef Basic_kernel::Flag Flag;

			constexpr Principal(): _parent(nullptr), _principal(nullptr) {}
			virtual ~Principal() {}

			const Ref& principal() const { return _principal; }
			Ref principal() { return _principal; }
			void principal(Ref rhs) { _principal = rhs; }
			void principal(This* rhs) { _principal = rhs; }

			size_t hash() const {
				return _principal && _principal->identifiable()
					? _principal->id()
					: size_t(_principal.ptr()) / alignof(size_t);
			}

			const Ref& parent() const { return _parent; }
			Ref parent() { return _parent; }
			void parent(Ref p) { _parent = p; }
			void parent(This* p) { _parent = p; }

			bool moves_upstream() const { return this->result() == Result::UNDEFINED && !_principal && _parent; }
			bool moves_downstream() const { return this->result() != Result::UNDEFINED && _principal && _parent; }
			bool moves_somewhere() const { return this->result() == Result::UNDEFINED && _principal && _parent; }
			bool moves_everywhere() const { return !_principal && !_parent; }

			void read_impl(Foreign_stream& in) {
				if (_parent) {
					std::stringstream s;
					s << "Parent is not null while reading from the data stream. Parent=";
					s << _parent;
					throw Error(s.str(), __FILE__, __LINE__, __func__);
				}
				Id parent_id;
				in >> parent_id;
				Logger<Level::KERNEL>() << "READING PARENT " << parent_id << std::endl;
				if (parent_id != ROOT_ID) {
					_parent = parent_id;
				}
				if (_principal.ptr() != nullptr) {
					throw Error("Principal kernel is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				Id principal_id;
				in >> principal_id;
				Logger<Level::KERNEL>() << "READING PRINCIPAL " << principal_id << std::endl;
				_principal = principal_id;
			}

			void write_impl(Foreign_stream& out) {
				out << (!_parent ? ROOT_ID : _parent->id());
				out << (!_principal ? ROOT_ID : _principal->id());
			}

			virtual void react(This*) {
				std::stringstream msg;
				msg << "Empty react in ";
				if (const Type<This>* tp = type()) {
					msg << *tp;
				} else {
					msg << "unknown type";
				}
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}

			virtual void error(This* rhs) { react(rhs); }

			virtual const Type<This>* type() const { return nullptr; }

			void run_act() {
				switch (this->result()) {
					case Result::UNDEFINED:
						if (_principal) {
							_principal->react(this);
						} else {
							this->act();
//							if (this->moves_everywhere()) {
//								delete this;
//							}
						}
						break;
					case Result::SUCCESS:
					case Result::ENDPOINT_NOT_CONNECTED:
					case Result::NO_UPSTREAM_SERVERS_LEFT:
					case Result::NO_PRINCIPAL_FOUND:
					case Result::USER_ERROR:
					default:
						Logger<Level::KERNEL>() << "Result is defined" << std::endl;
						if (!_principal) {
							Logger<Level::KERNEL>() << "Principal is null" << std::endl;
							if (!_parent) {
								delete this;
								Logger<Level::KERNEL>() << "SHUTDOWN" << std::endl;
								components::stop_all_factories();
							}
						} else {
							Logger<Level::KERNEL>() << "Principal is not null" << std::endl;
							bool del = *_principal == *_parent;
							if (this->result() == Result::SUCCESS) {
								Logger<Level::KERNEL>() << "Principal react" << std::endl;
								_principal->react(this);
							} else {
								Logger<Level::KERNEL>() << "Principal error" << std::endl;
								_principal->error(this);
							}
							if (del) {
								Logger<Level::KERNEL>() << "Deleting kernel " << *this << std::endl;
								delete this;
							}
						}
				}
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << '('
					<< (rhs.moves_upstream()   ? 'U' : ' ')
					<< (rhs.moves_downstream() ? 'D' : ' ') 
					<< (rhs.moves_somewhere()  ? 'S' : ' ') 
					<< (rhs.moves_everywhere()  ? 'B' : ' ') 
					<< ','
					<< rhs.id() << ','
					<< rhs.from() << ','
					<< rhs.to() << ','
					<< rhs.result()
					<< ')';
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
			inline void commit(S* srv, Result res = Result::SUCCESS) {
				this->principal(_parent);
				this->result(res);
				srv->send(this);
			}

			template<class It>
			void mark_as_deleted(It result) {
				if (!this->isset(Flag::DELETED)) {
					this->setf(Flag::DELETED);
					if (this->_parent) {
						this->_parent->mark_as_deleted(result);
					}
					*result = std::unique_ptr<This>(this);
					++result;
				}
			}

		private:
			Kernel_ref<This> _parent;
			Kernel_ref<This> _principal;
		};

		template<class K>
		struct Mobile: public K {
		
			constexpr Mobile(): _src(), _dst() {}

			virtual void read(Foreign_stream& in) { 
				typedef std::underlying_type<Result>::type Raw_result;
//				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				Raw_result r;
				in >> r;
				this->result(static_cast<Result>(r));
				Logger<Level::KERNEL>() << "Reading result = " << r << std::endl;
				in >> _id;
			}

			virtual void write(Foreign_stream& out) {
				typedef std::underlying_type<Result>::type Raw_result;
//				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				Raw_result r = static_cast<Raw_result>(this->result());
				Logger<Level::KERNEL>() << "Writing result = " << r << std::endl;
				out << r;
				Logger<Level::KERNEL>() << "Writing id = " << _id << std::endl;
				out << _id;
			}

			virtual void read_impl(Foreign_stream&) {}
			virtual void write_impl(Foreign_stream&) {}

			constexpr Id id() const { return _id; }
			void id(Id rhs) { _id = rhs; }
			constexpr bool identifiable() const { return _id != ROOT_ID; }

			virtual Endpoint from() const { return _src; }
			virtual void from(Endpoint rhs) { _src = rhs; }

			virtual Endpoint to() const { return _dst; }
			virtual void to(Endpoint rhs) { _dst = rhs; }

			constexpr bool operator==(const Mobile<K>& rhs) const {
				return this == &rhs || (id() != ROOT_ID && rhs.id() != ROOT_ID && id() == rhs.id());
			}

		private:
			Id _id = ROOT_ID;
			Endpoint _src;
			Endpoint _dst;
		};

	}

}
#else
namespace factory {

	class Notification: public Kernel {};

	template<class F, class G, class I>
	class Map: public Kernel {
	public:
		Map(F f_, G g_, I a_, I b_, I bs_=1):
			f(f_), g(g_), a(a_), b(b_), bs(bs_), n(0), m(calc_m()) {}
	
		bool is_profiled() const { return false; }
	
		struct Worker: public factory::Kernel {
			Worker(F& f_, I a_, I b_):
				f(f_), a(a_), b(b_) {}
			void act() {
				for (I i=a; i<b; ++i) f(i);
				commit(the_server());
			}
			F& f;
			I a, b;
		};
	
		void act() {
			for (I i=a; i<b; i+=bs) upstream(the_server(), new Worker(f, i, std::min(i+bs, b)));
		}
	
		void react(factory::Kernel* kernel) {
			Worker* w = reinterpret_cast<Worker*>(kernel);
			I x1 = w->a, x2 = w->b;
			for (I i=x1; i<x2; ++i) g(i);
			if (++n == m) commit(the_server());
		}
	
	private:
		I calc_m() const { return (b-a)/bs + ((b-a)%bs == 0 ? 0 : 1); }
	
	private:
		F f;
		G g;
		I a, b, bs, n, m;
	};
	
	
	template<class F, class G, class I>
	Map<F, G, I>* mapreduce(F f, G g, I a, I b, I bs=1) {
		return new Map<F, G, I>(f, g, a, b, bs);
	}

}
#endif
