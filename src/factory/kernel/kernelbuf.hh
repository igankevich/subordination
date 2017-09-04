#ifndef FACTORY_KERNEL_KERNELBUF_HH
#define FACTORY_KERNEL_KERNELBUF_HH

#include <type_traits>

#include <unistdx/base/packetbuf>
#include <unistdx/net/byte_order>

namespace factory {

	template<class Base>
	class basic_kernelbuf: public Base {

	public:
		typedef Base base_type;
		using typename base_type::traits_type;
		using typename base_type::char_type;
		typedef uint32_t portable_size_type;

		typedef sys::basic_packetbuf<char_type,traits_type> good_base_type;
		static_assert(
			std::is_base_of<good_base_type, Base>::value,
			"bad base class"
		);

	private:
		typedef sys::bytes<portable_size_type, char_type> bytes_type;

	public:
		basic_kernelbuf() {
			this->set_oheader(header_size());
			this->set_iheader(header_size());
		}

		virtual ~basic_kernelbuf() = default;

		basic_kernelbuf(basic_kernelbuf&&) = delete;
		basic_kernelbuf(const basic_kernelbuf&) = delete;
		basic_kernelbuf& operator=(basic_kernelbuf&&) = delete;
		basic_kernelbuf& operator=(const basic_kernelbuf&) = delete;

	private:

		std::streamsize
		xgetheader() override {
			bytes_type size(this->gptr(), this->gptr() + this->header_size());
			size.to_host_format();
			return size.value();
		}

		void
		put_header() override {
			bytes_type hdr(0);
			hdr.to_network_format();
			this->xsputn(hdr.begin(), hdr.size());
		}

		void
		overwrite_header(std::streamsize s) override {
			bytes_type hdr(s);
			hdr.to_network_format();
			traits_type::copy(this->opacket_begin(), hdr.begin(), hdr.size());
		}

		static constexpr std::streamsize
		header_size() {
			return sizeof(portable_size_type);
		}

	};

}

#endif // vim:filetype=cpp
