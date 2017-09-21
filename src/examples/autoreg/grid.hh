#ifndef EXAMPLES_AUTOREG_GRID_HH
#define EXAMPLES_AUTOREG_GRID_HH

namespace autoreg {

	struct Surface_part {

		Surface_part() = default;

		inline
		Surface_part(uint32_t i, uint32_t t0__, uint32_t t1__):
		_part(i),
		_t0(t0__),
		_t1(t1__)
		{}

		inline uint32_t
		part() const noexcept {
			return this->_part;
		}

		inline uint32_t
		t0() const noexcept {
			return this->_t0;
		}

		inline uint32_t
		t1() const noexcept {
			return this->_t1;
		}

		inline uint32_t
		part_size() const noexcept {
			return this->t1()-this->t0();
		}

		friend std::ostream&
		operator<<(std::ostream& out, const Surface_part& rhs) {
			out << "part=" << rhs.part() << ',';
			out << "t0=" << rhs.t0() << ',';
			out << "t1=" << rhs.t1() << ',';
			out << "part_size=" << rhs.part_size();
			return out;
		}

		friend sys::pstream&
		operator<<(sys::pstream& out, const Surface_part& rhs) {
			return out << rhs._part << rhs._t0 << rhs._t1;
		}

		friend sys::pstream&
		operator>>(sys::pstream& in, Surface_part& rhs) {
			return in >> rhs._part >> rhs._t0 >> rhs._t1;
		}

	private:

		uint32_t _part;
		uint32_t _t0;
		uint32_t _t1;

	};

	struct Uniform_grid {

		Uniform_grid() = default;

		inline
		Uniform_grid(uint32_t sz, uint32_t npts):
		_size(sz),
		_nparts(npts)
		{}

		inline Surface_part
		part(uint32_t i) const noexcept {
			return Surface_part(
				i,
				i*this->part_size(),
				(i == this->num_parts() - 1)
				?  this->_size
				: (i+1)* this->part_size()
			);
		}

		inline uint32_t
		num_parts() const noexcept {
			return this->_nparts;
		}

		friend sys::pstream&
		operator<<(sys::pstream& stream, const Uniform_grid& rhs) {
			return stream << rhs._size << rhs._nparts;
		}

		friend sys::pstream&
		operator>>(sys::pstream& stream, Uniform_grid& rhs) {
			return stream >> rhs._size >> rhs._nparts;
		}

	private:

		inline uint32_t
		part_size() const noexcept {
			return this->_size / this->num_parts();
		}

		uint32_t _size;
		uint32_t _nparts;

	};

}

#endif // vim:filetype=cpp
