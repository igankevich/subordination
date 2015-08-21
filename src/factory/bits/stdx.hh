namespace factory {

	namespace bits {

		template<size_t No, class F>
		struct Apply_to {
			constexpr explicit Apply_to(F&& f): func(f) {}
			template<class Arg>
			auto operator()(const Arg& rhs) ->
				typename std::result_of<F&(decltype(std::get<No>(rhs)))>::type
			{
				return func(std::get<No>(rhs));
			}
		private:
			F&& func;
		};

		template<class Mutex>
		struct no_lock {
			
			constexpr no_lock(Mutex& m) noexcept {}
			~no_lock() noexcept = default;
			inline void lock() noexcept {}
			inline void unlock() noexcept {}

			// disallow copy & move operations
			no_lock() = delete;
			no_lock(const no_lock&) = delete;
			no_lock(no_lock&&) = delete;
			no_lock& operator=(const no_lock&) = delete;
			no_lock& operator=(no_lock&&) = delete;

		};

		struct no_mutex {
			
			constexpr no_mutex() noexcept = default;
			~no_mutex() noexcept = default;
			inline void lock() noexcept {}
			inline bool try_lock() noexcept { return true; }
			inline void unlock() noexcept {}

			// disallow copy & move operations
			no_mutex(const no_mutex&) = delete;
			no_mutex(no_mutex&&) = delete;
			no_mutex& operator=(const no_mutex&) = delete;
			no_mutex& operator=(no_mutex&&) = delete;

		};

		struct no_semaphore {

			constexpr no_semaphore() noexcept = default;
			~no_semaphore() = default;

			template<class Lock>
			inline void wait(Lock&) {}

			template<class Lock, class Pred>
			inline void wait(Lock&, Pred) {}

			template<class Lock, class Rep>
			inline std::cv_status
			wait_for(Lock&, const Rep&) {
				return std::cv_status::no_timeout;
			}

			template<class Lock, class Duration, class Pred>
			inline bool
			wait_for(Lock&, const Duration&, Pred) {
				return true;
			}

			template<class Lock, class Duration>
			inline std::cv_status
			wait_until(Lock&, const Duration&) {
				return std::cv_status::no_timeout;
			}

			template<class Lock, class Clock, class Duration, class Pred>
			inline bool
			wait_until(Lock&, const Duration&, Pred) {
				return true;
			}

			inline void notify_one() noexcept {}
			inline void notify_all() noexcept {}

		};

		static_assert(std::is_empty<no_lock<int>>::value, "bad no_lock type");
		static_assert(std::is_empty<no_mutex>::value, "bad no_mutex type");
		static_assert(std::is_empty<no_semaphore>::value, "bad no_semaphore type");

		constexpr char
		const_tolower(char ch) noexcept {
			return ch >= 'A' && ch <= 'F' ? ('a'+ch-'A') : ch;
		}

		template<unsigned int radix, class Ch>
		constexpr Ch
		to_int(Ch ch) noexcept {
			return radix == 16 && ch >= 'a' ? ch-'a'+10 : ch-'0';
		}

		inline int
		get_stream_radix(const std::ios_base& out) noexcept {
			return out.flags() & std::ios_base::hex ? 16 :
				out.flags() & std::ios_base::oct ? 8 : 10;
		}

		template<class Uint, unsigned int radix, class It>
		constexpr Uint
		do_parse_uint(It first, It last, Uint val=0) noexcept {
			return first == last ? val
				: do_parse_uint<Uint, radix>(first+1, last, 
				val*radix + to_int<radix>(const_tolower(*first)));
		}

		template<class Uint, std::size_t n, class Arr>
		constexpr Uint
		parse_uint(Arr arr) noexcept {
			return
				n >= 2 && arr[0] == '0' && arr[1] == 'x' ? do_parse_uint<Uint,16>(arr+2, arr + n) :
				n >= 2 && arr[0] == '0' && arr[1] >= '1' && arr[1] <= '9' ? do_parse_uint<Uint,8>(arr+1, arr + n) :
				do_parse_uint<Uint,10>(arr, arr+n);
		}

		struct debug_stream {
			constexpr explicit debug_stream(const std::ios& s): str(s) {}
			friend std::ostream& operator<<(std::ostream& out, const debug_stream& rhs) {
				std::ostream::sentry s(out);
				if (s) {
					out
						<< (rhs.str.good() ? 'g' : '-')
						<< (rhs.str.bad()  ? 'b' : '-')
						<< (rhs.str.fail() ? 'f' : '-')
						<< (rhs.str.eof()  ? 'e' : '-');
				}
				return out;
			}
		private:
			const std::ios& str;
		};
	
	}

}
