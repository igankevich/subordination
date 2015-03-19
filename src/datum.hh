unsigned long time_seed() {
	return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

template<class T>
void rnd(T& val) {
	typedef typename
		std::conditional<std::is_floating_point<T>::value,
			std::uniform_real_distribution<T>,
				std::uniform_int_distribution<T>>::type
					Distribution;
	static std::default_random_engine generator(time_seed());
	static Distribution distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	val = distribution(generator);
}

struct Datum {

	Datum() {
		rnd(x); rnd(y); rnd(z);
		rnd(u); rnd(v); rnd(w);
	}

	Datum(const Datum& rhs):
		x(rhs.x), y(rhs.y), z(rhs.z),
		u(rhs.u), v(rhs.v), w(rhs.w)
	{}

	bool operator==(const Datum& rhs) const {
		return
			x == rhs.x && y == rhs.y && z == rhs.z &&
			u == rhs.u && v == rhs.v && w == rhs.w;
	}

	bool operator!=(const Datum& rhs) const {
		return
			x != rhs.x || y != rhs.y || z != rhs.z ||
			u != rhs.u || v != rhs.v || w != rhs.w;
	}

	friend Foreign_stream& operator<<(Foreign_stream& out, const Datum& rhs) {
		return out
			<< rhs.x << rhs.y << rhs.z
			<< rhs.u << rhs.v << rhs.w;
	}

	friend Foreign_stream& operator>>(Foreign_stream& in, Datum& rhs) {
		return in
			>> rhs.x >> rhs.y >> rhs.z
			>> rhs.u >> rhs.v >> rhs.w;
	}

	friend std::ostream& operator<<(std::ostream& out, const Datum& rhs) {
		return out
			<< rhs.x << rhs.y << rhs.z
			<< rhs.u << rhs.v << rhs.w;
	}

	static size_t real_size() {
		return
			sizeof(x) + sizeof(y) + sizeof(z) +
			sizeof(u) + sizeof(v) + sizeof(w);
	}

private:
	int8_t x;
	int16_t y;
	int32_t z;
	int64_t u;
	float v;
	double w;
};