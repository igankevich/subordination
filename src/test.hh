#include <string>
#include <random>
#include <functional>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <stdexcept>

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

	template<class X, class Y>
	void equal(X x, Y y) {
		if (!(x == y)) {
			std::stringstream msg;
			msg << "values are not equal: x="
				<< x << ", y=" << y;
			throw std::runtime_error(msg.str());
		}
	}

	template<class Container1, class Container2>
	void compare(const Container1& cnt1, const Container2& cnt2) {
		using namespace factory;
		auto pair = std::mismatch(cnt1.begin(), cnt1.end(), cnt2.begin());
		if (pair.first != cnt1.end()) {
			auto pos = pair.first - cnt1.begin();
			std::stringstream msg;
			msg << "input and output does not match at i=" << pos << ":\n'"
				<< *pair.first << "'\n!=\n'" << *pair.second << "'";
			throw std::runtime_error(msg.str());
		}
	}

	typedef std::chrono::nanoseconds::rep Time;
	
	Time time_seed() {
		return std::chrono::high_resolution_clock::now().time_since_epoch().count();
	}

	template<class T>
	T randomval() {
		typedef typename
			std::conditional<std::is_floating_point<T>::value,
				std::uniform_real_distribution<T>,
					std::uniform_int_distribution<T>>::type
						Distribution;
		typedef std::default_random_engine::result_type Res_type;
		static std::default_random_engine generator(static_cast<Res_type>(time_seed()));
		static Distribution distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
		return distribution(generator);
	}

}

namespace std {
	std::ostream& operator<<(std::ostream& out, const std::basic_string<unsigned char>& rhs) {
		std::ostream_iterator<char> it(out, "");
		std::copy(rhs.begin(), rhs.end(), it);
		return out;
	}
}

namespace factory {
	typedef std::chrono::nanoseconds::rep Time;
	typedef std::chrono::nanoseconds Nanoseconds;
	typedef typename std::make_signed<Time>::type Skew;

	static Time current_time_nano() {
		using namespace std::chrono;
		typedef std::chrono::steady_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
	}
}
