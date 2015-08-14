typedef std::chrono::nanoseconds::rep Time;

Time time_seed() {
	return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

template<class T>
void rnd(T& val) {
	typedef typename
		std::conditional<std::is_floating_point<T>::value,
			std::uniform_real_distribution<T>,
				std::uniform_int_distribution<T>>::type
					Distribution;
	typedef std::default_random_engine::result_type Res_type;
	static std::default_random_engine generator(static_cast<Res_type>(time_seed()));
	static Distribution distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	val = distribution(generator);
}

struct Datum {

	Datum():
		x(0), y(0), z(0),
		u(0), v(0), w(0)
	{
		rnd(x); rnd(y); rnd(static_cast<float&>(z));
		rnd(static_cast<double&>(u)); rnd(v); rnd(w);
	}

	constexpr Datum(const Datum& rhs):
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

	friend factory::packstream& operator<<(factory::packstream& out, const Datum& rhs) {
		return out
			<< rhs.x << rhs.y << rhs.z
			<< rhs.u << rhs.v << rhs.w;
	}

	friend factory::packstream& operator>>(factory::packstream& in, Datum& rhs) {
		return in
			>> rhs.x >> rhs.y >> rhs.z
			>> rhs.u >> rhs.v >> rhs.w;
	}

	friend std::ostream& operator<<(std::ostream& out, const Datum& rhs) {
		out.write(reinterpret_cast<const char*>(&rhs.x), sizeof(rhs.x));
		out.write(reinterpret_cast<const char*>(&rhs.y), sizeof(rhs.y));
		out.write(reinterpret_cast<const char*>(&rhs.z), sizeof(rhs.z));
		out.write(reinterpret_cast<const char*>(&rhs.u), sizeof(rhs.u));
		out.write(reinterpret_cast<const char*>(&rhs.v), sizeof(rhs.v));
		out.write(reinterpret_cast<const char*>(&rhs.w), sizeof(rhs.w));
		return out;
	}

	friend std::istream& operator>>(std::istream& in, Datum& rhs) {
		in.read(reinterpret_cast<char*>(&rhs.x), sizeof(rhs.x));
		in.read(reinterpret_cast<char*>(&rhs.y), sizeof(rhs.y));
		in.read(reinterpret_cast<char*>(&rhs.z), sizeof(rhs.z));
		in.read(reinterpret_cast<char*>(&rhs.u), sizeof(rhs.u));
		in.read(reinterpret_cast<char*>(&rhs.v), sizeof(rhs.v));
		in.read(reinterpret_cast<char*>(&rhs.w), sizeof(rhs.w));
		return in;
	}

	constexpr static size_t real_size() {
		return
			sizeof(x) + sizeof(y) + sizeof(z) +
			sizeof(u) + sizeof(v) + sizeof(w);
	}

private:
	int64_t x;
	int32_t y;
	factory::bits::Bytes<float> z;
	factory::bits::Bytes<double> u;
	int16_t v;
	int8_t w;
	char padding[5] = {};
};
