#ifndef FACTORY_KERNEL_STREAM_HH
#define FACTORY_KERNEL_STREAM_HH

#include <cassert>

#include <stdx/log.hh>
#include <sysx/packetstream.hh>
#include <factory/type.hh>
#include <factory/error.hh>

namespace factory {

	template<class T>
	struct Kernel_stream: public sysx::packetstream {

		typedef T kernel_type;
		typedef typename kernel_type::app_type app_type;
		typedef std::function<void(app_type,Kernel_stream&)> forward_func;
		typedef components::Types<Type<kernel_type>> types;
		typedef stdx::log<Kernel_stream> this_log;

		enum struct state {
			good = 0,
			partial_packet,
			bad_kernel
		};

		using sysx::packetstream::operator<<;
		using sysx::packetstream::operator>>;

		Kernel_stream() = default;
		Kernel_stream(stdx::packetbuf* buf): sysx::packetstream(buf) {}
		Kernel_stream(Kernel_stream&&) = default;

		Kernel_stream&
		operator<<(kernel_type& kernel) {
			const Type<kernel_type> type = kernel.type();
			if (!type) {
				std::stringstream msg;
				msg << "Can not find type for kernel id=" << kernel.id();
				throw components::Error(msg.str(), __FILE__, __LINE__, __func__);
			}
			begin_packet();
			*this << kernel.app() << type.id();
			kernel.write(*this);
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
					_doforward(app, *this);
				} else {
					assert(_types != nullptr);
					try {
						kernel = Type<kernel_type>::read_object(*_types, *this);
					} catch (components::Marshalling_error& err) {
						setstate(state::bad_kernel);
						this_log() << err << std::endl;
					}
					// eat remaining bytes
					rdbuf()->skip_packet();
				}

			}
			return *this;
		}

		void
		setapp(app_type rhs) noexcept {
			_thisapp = rhs;
		}

		void
		setforward(forward_func rhs) noexcept {
			_doforward = rhs;
		}

		void
		settypes(types* rhs) noexcept {
			_types = rhs;
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
		types* _types = nullptr;
		state _rdstate = state::good;

	};

}

#endif // FACTORY_KERNEL_STREAM_HH
