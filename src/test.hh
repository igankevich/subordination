namespace test {
	template<class T>
	std::basic_string<T> random_string(size_t size, T min = std::numeric_limits<T>::min(),
		T max = std::numeric_limits<T>::max()) {
		static std::default_random_engine generator(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()
		);
		std::uniform_int_distribution<T> dist(min, max);
		auto gen = std::bind(dist, generator);
		std::basic_string<T> ret(size, '_');
		std::generate(ret.begin(), ret.end(), gen);
		return ret;
	}
}
