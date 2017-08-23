#ifndef TEST_TEST_HH
#define TEST_TEST_HH

#include <string>
#include <random>
#include <functional>
#include <algorithm>
#include <iterator>
#include <chrono>

#include <unistdx/base/n_random_bytes>

#include <gtest/gtest.h>

namespace test {

	template<class T>
	std::basic_string<T>
	random_string(
		size_t size,
		T min = std::numeric_limits<T>::min(),
		T max = std::numeric_limits<T>::max()
	) {
		static std::default_random_engine generator(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()
		);
		std::uniform_int_distribution<T> dist(min, max);
		auto gen = std::bind(dist, generator);
		std::basic_string<T> ret(size, '_');
		std::generate(ret.begin(), ret.end(), gen);
		return ret;
	}

	template<class It, class Engine>
	void
	randomise(It first, It last, Engine& rng) {
		typedef typename std::decay<decltype(*first)>::type value_type;
		while (first != last) {
			*first = sys::n_random_bytes<value_type>(rng);
			++first;
		}
	}

}

namespace std {
	std::ostream&
	operator<<(std::ostream& out, const std::basic_string<unsigned char>& rhs) {
		std::ostream_iterator<char> it(out, "");
		std::copy(rhs.begin(), rhs.end(), it);
		return out;
	}
}

#endif // TEST_TEST_HH
