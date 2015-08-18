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

		static_assert(std::is_empty<no_lock<int>>::value, "bad no_lock type");
		static_assert(std::is_empty<no_mutex>::value, "bad no_mutex type");
	
	}

}
