#ifndef FACTORY_KERNEL_STREAM_HH
#define FACTORY_KERNEL_STREAM_HH

#include <cassert>

#include <sys/packetstream.hh>
#include <factory/type.hh>
#include <factory/error.hh>
#include <factory/reflection.hh>

namespace factory {

	template<class T>
	struct Kernel_stream: public sys::packetstream {

		typedef T kernel_type;
		typedef typename kernel_type::app_type app_type;
		typedef std::function<void(app_type)> forward_func;
		typedef Types types;

		enum struct state {
			good = 0,
			partial_packet,
			bad_kernel
		};

		using sys::packetstream::operator<<;
		using sys::packetstream::operator>>;

		Kernel_stream() = default;
		Kernel_stream(stdx::packetbuf* buf): sys::packetstream(buf) {}
		Kernel_stream(Kernel_stream&&) = default;

		Kernel_stream&
		operator<<(kernel_type* kernel) {
			return operator<<(*kernel);
		}

		Kernel_stream&
		operator<<(kernel_type& kernel) {
			const Type* type = ::factory::types.lookup(typeid(kernel));
			if (not type) {
				throw Bad_type(
					"no type is defined for the kernel",
					{__FILE__, __LINE__, __func__},
					kernel.id()
				);
			}
			begin_packet();
			*this << kernel.app() << type->id();
			kernel.write(*this);
			if (kernel.carries_parent()) {
				// embed parent into the current kernel
				kernel_type* parent = kernel.parent();
				assert(parent and "Trying to embed non-existent parent kernel.");
				const Type* parent_type = ::factory::types.lookup(typeid(*parent));
				assert(parent_type and "Trying to embed parent kernel with undefined type.");
				*this << parent_type->id();
				parent->write(*this);
			}
			end_packet();
			return *this;
		}

		Kernel_stream&
		operator>>(kernel_type*& kernel) {
			if (not read_packet()) {
				setstate(state::partial_packet);
			} else {
				app_type app;
				*this >> app;
				if (app != _thisapp) {
					_doforward(app);
				} else {
					try {
						kernel = static_cast<kernel_type*>(Types::read_object(::factory::types, *this));
						kernel->setapp(app);
						if (kernel->carries_parent()) {
							kernel_type* parent = static_cast<kernel_type*>(Types::read_object(::factory::types, *this));
							parent->setapp(app);
							kernel->parent(parent);
						}
					} catch (Bad_kernel& err) {
						setstate(state::bad_kernel);
						#ifndef NDEBUG
						stdx::dbg << err;
						#endif
					}
					// eat remaining bytes
					rdbuf()->skip_packet();
				}

			}
			return *this;
		}

		void
		setforward(forward_func rhs) noexcept {
			_doforward = rhs;
		}

		void
		setstate(state rhs) noexcept {
			_rdstate = rhs;
		}

		state
		rdstate() const noexcept {
			return _rdstate;
		}

		explicit
		operator bool() const noexcept {
			return good();
		}

		bool
		operator!() const noexcept {
			return !operator bool();
		}

		bool
		partial() const noexcept {
			return _rdstate == state::partial_packet;
		}

		bool
		good() const noexcept {
			return _rdstate == state::good;
		}

		bool
		bad() const noexcept {
			return _rdstate == state::bad_kernel;
		}

		void
		clear() noexcept {
			_rdstate = state::good;
		}

		void
		setapp(app_type rhs) noexcept {
			_thisapp = rhs;
		}

	private:

		friend std::ostream&
		operator<<(std::ostream& out, state rhs) {
			switch (rhs) {
				case state::good: out << "good"; break;
				case state::partial_packet: out << "partial_packet"; break;
				case state::bad_kernel: out << "bad_kernel"; break;
			}
			return out;
		}

		app_type _thisapp = 0;
		forward_func _doforward;
		state _rdstate = state::good;

	};

}

#endif // FACTORY_KERNEL_STREAM_HH
