#ifndef STDX_IOS_HH
#define STDX_IOS_HH

#include <ostream>

namespace stdx {

	template<class T, std::streamsize W>
	struct fixed_width {

		fixed_width(const T& x): val(x) {}

		friend std::ostream&
		operator<<(std::ostream& out, const fixed_width& rhs) {
			out.width(W);
			return out << rhs.val;
		}

	private:

		const T& val;

	};

	/**
	Save/restore std::ios flags and fill character with RAII
	upon construction/destruction of an ios_guard object.
	*/
	struct ios_guard {

		explicit
		ios_guard(std::ios& s):
		_stream(s),
		_oldf(_stream.flags()),
		_oldfill(_stream.fill())
		{}

		~ios_guard() {
			_stream.setf(_oldf);
			_stream.fill(_oldfill);
		}

	private:

		std::ios& _stream;
		std::ios_base::fmtflags _oldf;
		std::ios::char_type _oldfill;

	};

}

#endif // STDX_IOS_HH
