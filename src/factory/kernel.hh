#ifndef FACTORY_KERNEL_HH
#define FACTORY_KERNEL_HH

#include <bitset>

#include <stdx/log.hh>

#include <factory/server/basic_server.hh>
#include <sysx/process.hh>
#include <sysx/packetstream.hh>

namespace factory {

	namespace components {

		struct kernel_category {};

		typedef uint16_t result_type;

		enum struct Result: result_type {
			success = 0,
			undefined = 1,
			error = 2,
			endpoint_not_connected = 3,
			no_principal_found = 4
		};

		inline std::ostream&
		operator<<(std::ostream& out, Result rhs) {
			switch (rhs) {
				case Result::success: out << "success"; break;
				case Result::undefined: out << "undefined"; break;
				case Result::endpoint_not_connected: out << "endpoint_not_connected"; break;
				case Result::no_principal_found: out << "no_principal_found"; break;
				default: out << "unknown_result";
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
				return sysx::this_process::execute(this->_execpath);
			}

			bool
			operator<(const Application& rhs) const {
				return this->_id < rhs._id;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Application& rhs) {
				return stdx::format_fields(out, "exec", rhs._execpath);
			}

		private:
			path_type _execpath;
			id_type _id;
		};

		struct Basic_kernel {

			typedef std::chrono::steady_clock Clock;
			typedef Clock::time_point Time_point;
			typedef Clock::duration Duration;
			typedef std::bitset<3> Flags;
			typedef stdx::log<Basic_kernel> this_log;

			enum struct Flag {
				DELETED = 0,
				carries_parent = 1,
				priority_service = 2
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

			bool
			carries_parent() const noexcept {
				return this->isset(Flag::carries_parent);
			}

		private:

			Result _result = Result::undefined;
			Time_point _at = Time_point(Duration::zero());
			Flags _flags = 0;

		};

		struct Kernel_header {

			typedef Application::id_type app_type;

			Kernel_header() = default;
			Kernel_header(const Kernel_header&) = default;
			Kernel_header& operator=(const Kernel_header&) = default;
			~Kernel_header() = default;

			sysx::endpoint
			from() const noexcept {
				return _src;
			}

			void
			from(sysx::endpoint rhs) noexcept {
				_src = rhs;
			}

			sysx::endpoint
			to() const noexcept {
				return _dst;
			}

			void
			to(sysx::endpoint rhs) noexcept {
				_dst = rhs;
			}

			app_type
			app() const noexcept {
				return this->_app;
			}

			void
			setapp(app_type rhs) noexcept {
				this->_app = rhs;
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Kernel_header& rhs) {
				return stdx::format_fields(out, "src", rhs._src,
					"dst", rhs._dst, "app", rhs._app);
			}

		private:
			sysx::endpoint _src{};
			sysx::endpoint _dst{};
			Application::id_type _app = 0;
		};

		struct Mobile_kernel: public Basic_kernel, public Kernel_header {

			typedef uint64_t id_type;
			static constexpr const id_type no_id = 0;

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

			id_type
			id() const noexcept {
				return _id;
			}

			void
			id(id_type rhs) noexcept {
				_id = rhs;
			}

			bool
			identifiable() const noexcept {
				return _id != no_id;
			}

			void
			set_id(id_type rhs) noexcept {
				_id = rhs;
			}

			bool
			operator==(const Mobile_kernel& rhs) const noexcept {
				return this == &rhs or (
					id() == rhs.id()
					and identifiable()
					and rhs.identifiable()
				);
			}

		private:

			id_type _id = no_id;

		};

		template<class Config>
		struct Principal: public Mobile_kernel {

			typedef Mobile_kernel base_kernel;
			typedef Basic_kernel::Flag Flag;
			typedef stdx::log<Principal> this_log;
//			typedef Managed_object<Server<Principal>> server_type;
			typedef typename Config::server server_type;
			typedef typename Config::factory factory_type;
			using Mobile_kernel::id_type;

			const Principal*
			principal() const {
				return _principal;
			}

			Principal*
			principal() {
				return _principal;
			}

			id_type
			principal_id() const {
				return _principal_id;
			}

			void
			set_principal_id(id_type id) {
				_principal_id = id;
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

			id_type
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

			bool
			moves_upstream() const noexcept {
				return this->result() == Result::undefined && !_principal && _parent;
			}

			bool
			moves_downstream() const noexcept {
				return this->result() != Result::undefined && _principal && _parent;
			}

			bool
			moves_somewhere() const noexcept {
				return this->result() == Result::undefined && _principal && _parent;
			}

			bool
			moves_everywhere() const noexcept {
				return !_principal && !_parent;
			}

			void
			read(sysx::packetstream& in) override {
				base_kernel::read(in);
				bool b = false;
				in >> b;
				if (b) {
					this->setf(Flag::carries_parent);
				}
				assert(not _parent and "Parent is not null while reading from the data stream.");
				in >> _parent_id;
				assert(not _principal and "Principal kernel is not null while reading from the data stream.");
				in >> _principal_id;
			}

			void
			write(sysx::packetstream& out) override {
				base_kernel::write(out);
				out << carries_parent();
				if (moves_downstream() or moves_somewhere()) {
					out << _parent_id << _principal_id;
				} else {
					out << (not _parent ? Mobile_kernel::no_id : _parent->id());
					out << (not _principal ? Mobile_kernel::no_id : _principal->id());
				}
			}

			virtual void
			act(server_type& this_server) {
				act();
			}

			virtual void
			act() {}

			virtual void
			react(server_type&, Principal* child) {
				react(child);
			}

			virtual void
			react(Principal*) {
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
				_server = &this_server;
				if (_principal) {
					_principal->_server = &this_server;
				}
				switch (this->result()) {
					case Result::undefined:
						if (_principal) {
							_principal->react(this_server, this);
						} else {
							this->act(this_server);
//							if (this->moves_everywhere()) {
//								delete this;
//							}
						}
						break;
					case Result::success:
					case Result::endpoint_not_connected:
					case Result::no_principal_found:
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
							if (this->result() == Result::success) {
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
					<< (rhs.moves_everywhere() ? 'b' : '-')
					<< ",tp=" << rhs.type()
					<< ",id=" << rhs.id()
					<< ",src=" << rhs.from()
					<< ",dst=" << rhs.to()
					<< ",rslt=" << rhs.result()
					<< ",app=" << rhs.app()
					<< ",parent=" << rhs._parent
					<< ",principal=" << rhs._principal
					<< '}';
			}

		public:

			factory_type*
			factory() noexcept {
				assert(_server != nullptr);
				return _server->factory();
			}

			server_type*
			local_server() noexcept {
				assert(_server != nullptr);
				return _server->local_server();
			}

			server_type*
			remote_server() noexcept {
				assert(_server != nullptr);
				return _server->remote_server();
			}

			template<class S>
			void
			upstream(S* this_server, Principal* a) {
				a->parent(this);
				this_server->send(a);
			}

			template<class S>
			void
			upstream_carry(S* this_server, Principal* a) {
				a->parent(this);
				a->setf(Flag::carries_parent);
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
				this->result(Result::success);
				this_server->send(a);
			}

			template<class S>
			void
			commit(S* srv, Result res = Result::success) {
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
				id_type _parent_id;
			};
			union {
				Principal* _principal = nullptr;
				id_type _principal_id;
			};
			server_type* _server = nullptr;

		};

	}

	template<class Base>
	struct Priority_kernel: public Base {
		Priority_kernel() {
			this->setf(components::Basic_kernel::Flag::priority_service);
		}
	};

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

#endif // FACTORY_KERNEL_HH
