#ifndef STDX_RANDOM_HH
#define STDX_RANDOM_HH

#include <algorithm>
#include <functional>

namespace stdx {

	template<class Engine, class Result>
	struct adapt_engine {

		typedef Result result_type;
		typedef typename Engine::result_type base_result_type;

		explicit
		adapt_engine(Engine& rhs):
		_engine(rhs),
		_it(end())
		{}

		result_type
		operator()() {
			if (_it == end()) {
				_it = begin();
				_result = _engine();
			}
			result_type res = *_it;
			++_it;
			return res;
		}

	private:

		result_type*
		begin() { return _buffer; }

		result_type*
		end() { return _buffer + sizeof(_buffer); }

		Engine& _engine;
		union {
		result_type _buffer[sizeof(base_result_type) / sizeof(result_type)];
		base_result_type _result;
		};

		result_type* _it;

		static_assert(sizeof(base_result_type) % sizeof(result_type) == 0,
			"bad result type");
	};

	template<class Result, class Engine>
	Result
	n_random_bytes(Engine& engine) {

		typedef Result result_type;
		typedef typename Engine::result_type base_result_type;

		static_assert(sizeof(base_result_type) > 0, "bad base result type");
		static_assert(sizeof(result_type) > 0, "bad result type");
		static_assert(sizeof(result_type) % sizeof(base_result_type) == 0, "bad result type");

		constexpr const std::size_t NUM_BASE_RESULTS =
			sizeof(result_type) / sizeof(base_result_type);
		union {
			result_type value{};
			base_result_type base[NUM_BASE_RESULTS];
		} result;
		std::generate_n(
			result.base,
			NUM_BASE_RESULTS,
			std::ref(engine)
		);
		return result.value;
	}

}

#endif // STDX_RANDOM_HH
