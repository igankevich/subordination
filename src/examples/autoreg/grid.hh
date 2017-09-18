#ifndef APPS_AUTOREG_GRID_HH
#define APPS_AUTOREG_GRID_HH

namespace autoreg {

	struct Surface_part {

		Surface_part() = default;

		Surface_part(uint32_t i, uint32_t t0__, uint32_t t1__):
		_part(i), _t0(t0__), _t1(t1__) {}

		uint32_t part() const { return _part; }
		uint32_t t0() const { return _t0; }
		uint32_t t1() const { return _t1; }
		uint32_t part_size() const { return t1()-t0(); }

		friend ostream&
		operator<<(ostream& out, const Surface_part& rhs) {
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

		Uniform_grid(uint32_t sz, uint32_t npts):
		size(sz), nparts(npts) {}

		Surface_part part(uint32_t i) const {
			return Surface_part(i, i*part_size(),
				(i == num_parts() - 1) ? size : (i+1)*part_size());
		}

		uint32_t num_parts() const { return nparts; }

		friend sys::pstream&
		operator<<(sys::pstream& stream, const Uniform_grid& rhs) {
			return stream << rhs.size << rhs.nparts;
		}

		friend sys::pstream&
		operator>>(sys::pstream& stream, Uniform_grid& rhs) {
			return stream >> rhs.size >> rhs.nparts;
		}

	private:

		uint32_t part_size() const { return size / num_parts(); }

		uint32_t size;
		uint32_t nparts;

	};

	struct Non_uniform_grid {

		Non_uniform_grid(std::size_t sz, std::size_t max_num_parts, std::size_t modulo_):
			size(sz), modulo(modulo_),
	//		nparts((-1 + sqrt(1+8*max_num_parts))/2),
			nparts(max_num_parts / num_parts(modulo) * modulo),
			min_part_size(size / max_num_parts) {}

		Surface_part part(std::size_t i) const {
			return Surface_part(i, (num_parts(modulo)*(i/modulo) + num_parts(i%modulo))*min_part_size,
				(i == num_parts() - 1) ? size : (num_parts(modulo)*((i+1)/modulo) + num_parts((i+1)%modulo))*min_part_size);
		}

		std::size_t num_parts() const { return nparts; }

	private:

	//	std::size_t num_parts_before(std::size_t i) const { return i%modulo * (i%modulo + 1) / 2; }
		std::size_t num_parts(std::size_t i) const { return i*(i+1)/2; }

		std::size_t size;
		std::size_t modulo;
		std::size_t nparts;
		std::size_t min_part_size;
	};

}

#endif // vim:filetype=cpp
