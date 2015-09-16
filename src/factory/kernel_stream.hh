#ifndef FACTORY_KERNEL_STREAM_HH
#define FACTORY_KERNEL_STREAM_HH

#include <sysx/packetstream.hh>
#include <factory/type.hh>

namespace factory {

	template<class T>
	struct Kernel_stream: public sysx::packetstream {

		typedef T kernel_type;
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
			return *this;
		}

	};

}

#endif // FACTORY_KERNEL_STREAM_HH
