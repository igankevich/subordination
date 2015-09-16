#ifndef STDX_PACKETBUF_HH
#define STDX_PACKETBUF_HH

#include <queue>

#include <stdx/streambuf.hh>

namespace stdx {

	template<class Ch, class Tr=std::char_traits<Ch>>
	struct basic_packetbuf: public basic_streambuf<Ch,Tr> {

		virtual void
		begin_packet() {}

		virtual void
		end_packet() {}

		virtual bool
		read_packet() { return false; }

	};
	typedef basic_packetbuf<char> packetbuf;

}

#endif // STDX_PACKETBUF_HH
