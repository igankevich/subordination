#ifndef FACTORY_KERNEL_HH
#define FACTORY_KERNEL_HH

#include <bitset>
#include <vector>

#include <factory/basic_server.hh>
#include <sys/process.hh>
#include <sys/packetstream.hh>
#include "result.hh"

namespace factory {

	struct Basic_kernel {

		typedef std::chrono::system_clock Clock;
		typedef Clock::time_point Time_point;
		typedef Clock::duration Duration;
		typedef std::bitset<3> Flags;

		/// Various kernel flags.
		enum struct Flag { DELETED = 0,
			/**
			Setting the flag tells \link factory::Kernel_stream\endlink that
			the kernel carries a backup copy of its parent to the subordinate
			node. This is the main mechanism of providing fault tolerance for
			principal kernels, it works only when <em>principal kernel have
			only one subordinate at a time</em>.
			*/
			carries_parent = 1,
			/**
			\deprecated This flag should not be used for purposes other than
			debugging.
			*/
			priority_service = 2 };

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

		#ifdef SPRINGY
		float _mass = 1.f;
		#endif

	protected:

		Result _result = Result::undefined;
		Time_point _at = Time_point(Duration::zero());
		Flags _flags = 0;

	};

	struct Kernel_header {

		typedef uint16_t app_type;

		Kernel_header() = default;
		Kernel_header(const Kernel_header&) = default;
		Kernel_header& operator=(const Kernel_header&) = default;
		~Kernel_header() = default;

		sys::endpoint
		from() const noexcept {
			return _src;
		}

		void
		from(sys::endpoint rhs) noexcept {
			_src = rhs;
		}

		sys::endpoint
		to() const noexcept {
			return _dst;
		}

		void
		to(sys::endpoint rhs) noexcept {
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

		bool
		is_foreign() const noexcept {
			return static_cast<bool>(_src);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Kernel_header& rhs) {
			return out << stdx::make_object(
				"src", rhs._src,
				"dst", rhs._dst,
				"app", rhs._app
			);
		}

	private:
		sys::endpoint _src{};
		sys::endpoint _dst{};
		app_type _app = 0;
	};

	struct Mobile_kernel: public Basic_kernel, public Kernel_header {

		typedef uint64_t id_type;
		static constexpr const id_type no_id = 0;

		virtual void
		read(sys::packetstream& in) {
			in >> _result >> _id;
		}

		virtual void
		write(sys::packetstream& out) {
			out << _result << _id;
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

		bool
		operator!=(const Mobile_kernel& rhs) const noexcept {
			return !operator==(rhs);
		}

	private:

		id_type _id = no_id;

	};

	class Kernel: public Mobile_kernel {

	public:
		typedef Mobile_kernel base_kernel;
		typedef Basic_kernel::Flag Flag;
		using Mobile_kernel::id_type;
		typedef std::vector<sys::endpoint> neighbours_type;

		const Kernel*
		principal() const {
			return _principal;
		}

		Kernel*
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
		principal(Kernel* rhs) {
			_principal = rhs;
		}

		const Kernel*
		parent() const {
			return _parent;
		}

		Kernel*
		parent() {
			return _parent;
		}

		id_type
		parent_id() const {
			return _parent_id;
		}

		void
		parent(Kernel* p) {
			_parent = p;
		}

		bool
		has_neighbour() const noexcept {
			return !_neighbours.empty();
		}

		const neighbours_type&
		neighbours() const noexcept {
			return _neighbours;
		}

		neighbours_type&
		neighbours() noexcept {
			return _neighbours;
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
		read(sys::packetstream& in) override {
			base_kernel::read(in);
			bool b = false;
			in >> b;
			if (b) {
				this->setf(Flag::carries_parent);
			}
			assert(not _parent and "Parent is not null while reading from the data stream.");
			in >> _parent_id;
			assert(not _principal and "Principal is not null while reading from the data stream.");
			in >> _principal_id;
			if (carries_parent()) {
				read_neighbours(in);
			}
		}

		void
		write(sys::packetstream& out) override {
			base_kernel::write(out);
			out << carries_parent();
			if (moves_downstream() or moves_somewhere()) {
				out << _parent_id << _principal_id;
			} else {
				out << (not _parent ? Mobile_kernel::no_id : _parent->id());
				out << (not _principal ? Mobile_kernel::no_id : _principal->id());
				if (carries_parent()) {
					write_neighbours(out);
				}
			}
		}

		inline void
		operator()() {
			this->act();
		}

		virtual void
		act() {}

		virtual void
		react(Kernel*) {
			std::stringstream msg;
			msg << "Empty react: type=" << typeid(*this).name();
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
		}

		virtual void
		error(Kernel* rhs) {
			react(rhs);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Kernel& rhs) {
			const char state[] = {
				(rhs.moves_upstream()   ? 'u' : '-'),
				(rhs.moves_downstream() ? 'd' : '-'),
				(rhs.moves_somewhere()  ? 's' : '-'),
				(rhs.moves_everywhere() ? 'b' : '-'),
				'\0'
			};
			return out << stdx::make_object(
				"state", state,
				"type", typeid(rhs).name(),
				"id", rhs.id(),
				"src", rhs.from(),
				"dst", rhs.to(),
				"ret", rhs.result(),
				"app", rhs.app(),
				"parent", rhs._parent,
				"principal", rhs._principal
			);
		}

	public:

		/// New API

		inline Kernel*
		call(Kernel* rhs) noexcept {
			rhs->parent(this);
			return rhs;
		}

		inline Kernel*
		carry_parent(Kernel* rhs) noexcept {
			rhs->parent(this);
			rhs->setf(Flag::carries_parent);
			return rhs;
		}

		inline void
		return_to_parent(Result ret = Result::success) noexcept {
			return_to(_parent, ret);
		}

		inline void
		return_to(Kernel* rhs, Result ret = Result::success) noexcept {
			this->principal(rhs);
			this->result(ret);
		}

		inline void
		recurse() noexcept {
			this->principal(this);
		}

		template<class It>
		void
		mark_as_deleted(It result) noexcept {
			if (!this->isset(Flag::DELETED)) {
				this->setf(Flag::DELETED);
				if (this->_parent) {
					this->_parent->mark_as_deleted(result);
				}
				*result = std::unique_ptr<Kernel>(this);
				++result;
			}
		}

	private:

		void
		write_neighbours(sys::packetstream& out) {
			out << uint32_t(_neighbours.size());
			for (const sys::endpoint& nbr : _neighbours) {
				out << nbr;
			}
		}

		void
		read_neighbours(sys::packetstream& in) {
			uint32_t n = 0;
			in >> n;
			_neighbours.resize(n);
			for (uint32_t i=0; i<n; ++i) {
				in >> _neighbours[i];
			}
		}

		union {
			Kernel* _parent = nullptr;
			id_type _parent_id;
		};
		union {
			Kernel* _principal = nullptr;
			id_type _principal_id;
		};
		neighbours_type _neighbours;

	};

	struct Application {

		typedef std::string path_type;

		static const Kernel_header::app_type ROOT = 0;

		Application() = default;

		explicit
		Application(const path_type& exec):
			_execpath(exec) {}

		int
		execute() const {
			return sys::this_process::execute(this->_execpath);
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Application& rhs) {
			return out << stdx::make_object("exec", rhs._execpath);
		}

	private:

		path_type _execpath;

	};

	template<class Base>
	struct Priority_kernel: public Base {
		Priority_kernel() {
			this->setf(Basic_kernel::Flag::priority_service);
		}
	};

}

#ifdef SPRINGY
#include <springy/springy.hh>
#include <test/demangle.hh>
namespace std {

	template<>
	struct hash<factory::Kernel> {

		typedef size_t result_type;
		typedef factory::Kernel argument_type;

		size_t
		operator()(const factory::Kernel& rhs) const noexcept {
			return rhs.identifiable() ? rhs.id() : size_t(std::addressof(rhs));
		}

	};

}
namespace springy {

	template<>
	struct Node<factory::Kernel> {

		explicit
		Node(const factory::Kernel& rhs) noexcept:
		_kernel(rhs)
		{}

		std::string
		label() const noexcept {
			return test::short_name(test::demangle_name(typeid(_kernel).name()));
		}

		float
		mass() const noexcept {
			return _kernel._mass;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Node& rhs) {
			std::hash<factory::Kernel> hsh;
			return out << 'n' << hsh(rhs._kernel);
		}

	private:

		const factory::Kernel& _kernel;

	};

}
#endif

#endif // FACTORY_KERNEL_HH
