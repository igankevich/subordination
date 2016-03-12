#ifndef STDX_INTERVAL_HH
#define STDX_INTERVAL_HH

#include <cassert>

namespace stdx {

	template<class I>
	struct interval {

		typedef I int_type;

		constexpr
		interval() noexcept:
		_start(0), _end(0) {}

		constexpr
		interval(I a, I b) noexcept:
		_start(a), _end(b) {}

		constexpr
		interval(const interval& rhs) noexcept:
		_start(rhs._start), _end(rhs._end) {}

		constexpr
		I start() const noexcept {
			return _start;
		}

		constexpr
		I end() const noexcept {
			return _end;
		}

		constexpr bool
		overlaps(const interval& rhs) const noexcept {
			return (_start < rhs._start && _end > rhs._start)
				|| (rhs._start < _start && rhs._end > _start);
		}

		inline interval&
		operator+=(const interval& rhs) noexcept {
			assert(overlaps(rhs));
			merge(rhs);
			return *this;
		}

		void
		merge(const interval& rhs) noexcept {
			_start = std::min(_start, rhs._start);
			_end = std::max(_end, rhs._end);
		}

		constexpr bool
		operator<(const interval& rhs) const noexcept {
			return _start < rhs._start;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const interval& rhs) {
			return out << rhs._start << ' ' << rhs._end;
		}

		friend std::istream&
		operator>>(std::istream& in, interval& rhs) {
			return in >> rhs._start >> rhs._end;
		}

		constexpr bool
		empty() const noexcept {
			return !(_start < _end);
		}

		constexpr I
		count() const noexcept {
			return _end - _start;
		}

	private:

		I _start, _end;

	};

}

#endif // STDX_INTERVAL_HH
