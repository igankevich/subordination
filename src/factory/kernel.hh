#ifndef FACTORY_KERNEL_HH
#define FACTORY_KERNEL_HH

#include <bitset>

#include <factory/server/basic_server.hh>
#include <sysx/process.hh>
#include <sysx/packetstream.hh>

#ifndef FACTORY_CONFIGURATION
namespace factory {

	namespace components {

		struct kernel_category {};

		typedef uint16_t result_type;

		enum struct Result: result_type {
			SUCCESS = 0,
			UNDEFINED = 1,
			ENDPOINT_NOT_CONNECTED = 3,
			NO_UPSTREAM_SERVERS_LEFT = 4,
			NO_PRINCIPAL_FOUND = 5,
			USER_ERROR = 6,
			FATAL_ERROR = 7
		};

		inline std::ostream&
		operator<<(std::ostream& out, Result rhs) {
			switch (rhs) {
				case Result::SUCCESS: out << "SUCCESS"; break;
				case Result::UNDEFINED: out << "UNDEFINED"; break;
				case Result::ENDPOINT_NOT_CONNECTED: out << "ENDPOINT_NOT_CONNECTED"; break;
				case Result::NO_UPSTREAM_SERVERS_LEFT: out << "NO_UPSTREAM_SERVERS_LEFT"; break;
				case Result::NO_PRINCIPAL_FOUND: out << "NO_PRINCIPAL_FOUND"; break;
				case Result::USER_ERROR: out << "USER_ERROR"; break;
				case Result::FATAL_ERROR: out << "FATAL_ERROR"; break;
				default: out << "UNKNOWN_RESULT";
			}
			return out;
		}

		struct Application {

			typedef uint16_t id_type;
			typedef std::string path_type;

			static const id_type ROOT = 0;

			Application(): _execpath(), _id(ROOT) {}

			explicit
			Application(const path_type& exec, id_type id):
				_execpath(exec), _id(id) {}

			id_type
			id() const {
				return this->_id;
			}

			int
			execute() const {
				sysx::this_process::env("APP_ID", this->_id);
				return sysx::this_process::execute(this->_execpath);
			}

			bool
			operator<(const Application& rhs) const {
				return this->_id < rhs._id;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Application& rhs) {
				return out
					<< "{exec=" << rhs._execpath
					<< ",id=" << rhs._id << '}';
			}

		private:
			path_type _execpath;
			id_type _id;
		};

		struct Basic_kernel {

			typedef std::chrono::steady_clock Clock;
			typedef Clock::time_point Time_point;
			typedef Clock::duration Duration;
			typedef std::bitset<1> Flags;
			typedef Application::id_type app_type;
			typedef stdx::log<Basic_kernel> this_log;
			
			enum struct Flag {
				DELETED = 0
			};

			virtual
			~Basic_kernel() = default;

			Result
			result() const noexcept {
				return _result;
			}

			void
			result(Result rhs) noexcept {
				_result = rhs;
			}

			// timed kernels
			Time_point
			at() const noexcept {
				return _at;
			}

			void
			at(Time_point t) noexcept {
				_at = t;
			}

			void
			after(Duration delay) noexcept {
				_at = Clock::now() + delay;
			}

			bool
			timed() const noexcept {
				return _at != Time_point(Duration::zero());
			}

			// flags
			Flags
			flags() const noexcept {
				return _flags;
			}

			void
			setf(Flag f) noexcept {
				_flags.set(static_cast<size_t>(f));
			}

			void
			unsetf(Flag f) noexcept {
				_flags.reset(static_cast<size_t>(f));
			}

			bool
			isset(Flag f) const noexcept {
				return _flags.test(static_cast<size_t>(f));
			}

		private:

			Result _result = Result::UNDEFINED;
			Time_point _at = Time_point(Duration::zero());
			Flags _flags = 0;
		};

		struct Mobile_kernel: public Basic_kernel {

			virtual void
			read(sysx::packetstream& in) { 
				typedef std::underlying_type<Result>::type Raw_result;
				Raw_result r;
				in >> r;
				this->result(static_cast<Result>(r));
				in >> _id;
			}

			virtual void
			write(sysx::packetstream& out) {
				typedef std::underlying_type<Result>::type Raw_result;
				Raw_result r = static_cast<Raw_result>(this->result());
				out << r << this->_id;
			}

			Id
			id() const noexcept {
				return _id;
			}

			void
			id(Id rhs) noexcept {
				_id = rhs;
			}

			bool
			identifiable() const noexcept {
				return _id != ROOT_ID;
			}

			virtual sysx::endpoint
			from() const noexcept {
				return _src;
			}

			virtual void
			from(sysx::endpoint rhs) noexcept {
				_src = rhs;
			}

			virtual sysx::endpoint
			to() const noexcept {
				return _dst;
			}

			virtual void
			to(sysx::endpoint rhs) noexcept {
				_dst = rhs;
			}

			bool
			operator==(const Mobile_kernel& rhs) const noexcept {
				return this == &rhs || (id() != ROOT_ID && rhs.id() != ROOT_ID && id() == rhs.id());
			}

			app_type
			app() const noexcept {
				return this->_app;
			} 

			void
			setapp(app_type rhs) noexcept {
				this->_app = rhs;
			}

		private:

			Id _id = ROOT_ID;
			sysx::endpoint _src{};
			sysx::endpoint _dst{};
			Application::id_type _app = 0;

		};

		template<class Config>
		struct Principal: public Mobile_kernel {

			typedef Mobile_kernel base_kernel;
			typedef Basic_kernel::Flag Flag;
			typedef stdx::log<Principal> this_log;
//			typedef Managed_object<Server<Principal>> server_type;
			typedef typename Config::server server_type;

			const Principal*
			principal() const {
				return _principal;
			}

			Principal*
			principal() {
				return _principal;
			}

			Id
			principal_id() const {
				return _principal_id;
			}

			void
			principal(Principal* rhs) {
				_principal = rhs;
			}

			const Principal*
			parent() const {
				return _parent;
			}

			Principal*
			parent() {
				return _parent;
			}

			Id
			parent_id() const {
				return _parent_id;
			}

			void
			parent(Principal* p) {
				_parent = p;
			}

			size_t
			hash() const {
				return _principal && _principal->identifiable()
					? _principal->id()
					: size_t(_principal) / alignof(size_t);
			}

			bool moves_upstream() const { return this->result() == Result::UNDEFINED && !_principal && _parent; }
			bool moves_downstream() const { return this->result() != Result::UNDEFINED && _principal && _parent; }
			bool moves_somewhere() const { return this->result() == Result::UNDEFINED && _principal && _parent; }
			bool moves_everywhere() const { return !_principal && !_parent; }

			void
			read(sysx::packetstream& in) override {
				base_kernel::read(in);
				if (_parent) {
					std::stringstream s;
					s << "Parent is not null while reading from the data stream. Parent=";
					s << _parent;
					throw Error(s.str(), __FILE__, __LINE__, __func__);
				}
				in >> _parent_id;
				if (_principal) {
					throw Error("Principal kernel is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				in >> _principal_id;
			}

			void
			write(sysx::packetstream& out) override {
				base_kernel::write(out);
				if (moves_downstream()) {
					out << _parent_id << _principal_id;
				} else {
					out << (!_parent ? ROOT_ID : _parent->id());
					out << (!_principal ? ROOT_ID : _principal->id());
				}
			}

			virtual void
			act(server_type& this_server) {
				act();
			}

			virtual void
			act() {}

			virtual void
			react(server_type&, Principal*) {
				std::stringstream msg;
				msg << "Empty react in ";
				const Type<Principal> tp = type();
				if (tp) {
					msg << tp;
				} else {
					msg << "unknown type";
					msg << " typeid=" << typeid(*this).name();
				}
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}

			virtual void
			error(server_type& this_server, Principal* rhs) {
				react(this_server, rhs);
			}

			virtual const Type<Principal>
			type() const noexcept {
				return Type<Principal>{0};
			}

			void
			run_act(server_type& this_server) {
				switch (this->result()) {
					case Result::UNDEFINED:
						if (_principal) {
							_principal->react(this_server, this);
						} else {
							this->act(this_server);
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
						this_log() << "Result is defined" << std::endl;
						if (!_principal) {
							this_log() << "Principal is null" << std::endl;
							if (!_parent) {
								delete this;
								this_log() << "SHUTDOWN" << std::endl;
								this_server.factory()->shutdown();
							}
						} else {
							this_log() << "Principal is not null" << std::endl;
							bool del = *_principal == *_parent;
							if (this->result() == Result::SUCCESS) {
								_principal->react(this_server, this);
								this_log() << "Principal end react" << std::endl;
							} else {
								this_log() << "Principal error" << std::endl;
								_principal->error(this_server, this);
							}
							if (del) {
								this_log() << "delete " << *this << std::endl;
								delete this;
							}
						}
				}
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Principal& rhs) {
				return out << '{'
					<< (rhs.moves_upstream()   ? 'u' : '-')
					<< (rhs.moves_downstream() ? 'd' : '-') 
					<< (rhs.moves_somewhere()  ? 's' : '-') 
					<< (rhs.moves_everywhere()  ? 'b' : '-') 
					<< ",tp=" << rhs.type()
					<< ",id=" << rhs.id()
					<< ",src=" << rhs.from()
					<< ",dst=" << rhs.to()
					<< ",rslt=" << rhs.result()
					<< ",app=" << rhs.app()
					<< ",parent=" << rhs._principal
					<< ",principal=" << rhs._parent
					<< '}';
			}
		
		public:
			template<class S>
			void
			upstream(S* this_server, Principal* a) {
				a->parent(this);
				this_server->send(a);
			}

			template<class S>
			void
			downstream(S* this_server, Principal* a) {
				a->parent(this);
				a->principal(this);
				this_server->send(a);
			}

			template<class S>
			void
			downstream(S* this_server, Principal* a, Principal* b) {
				a->principal(b);
				this->result(Result::SUCCESS);
				this_server->send(a);
			}

			template<class S>
			void
			commit(S* srv, Result res = Result::SUCCESS) {
				this->principal(_parent);
				this->result(res);
				srv->send(this);
			}

			template<class It>
			void
			mark_as_deleted(It result) noexcept {
				if (!this->isset(Flag::DELETED)) {
					this_log() << "marked for death " << *this << std::endl;
					this->setf(Flag::DELETED);
					if (this->_parent) {
						this->_parent->mark_as_deleted(result);
					}
					*result = std::unique_ptr<Principal>(this);
					++result;
				}
			}

		private:
			union {
				Principal* _parent = nullptr;
				Id _parent_id;
			};
			union {
				Principal* _principal = nullptr;
				Id _principal_id;
			};
		};

	}

	using components::Result;

}

namespace stdx {

	template<class T>
	struct type_traits<factory::components::Principal<T>> {
		static constexpr const char*
		short_name() { return "kernel"; }
		typedef factory::components::kernel_category category;
	};

}
#else
/*
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
			Worker* w = dynamic_cast<Worker*>(kernel);
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
*/
#endif

#endif // FACTORY_KERNEL_HH
