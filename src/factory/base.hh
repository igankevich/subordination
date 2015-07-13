namespace factory {

	struct Null {};

	namespace components {
		void print_all_endpoints(std::ostream&);
		void stop_all_factories(bool now=false);
	}

//	typedef std::int64_t Time;
//
//	Time now() {
//		return std::chrono::steady_clock::now().time_since_epoch().count();
//	}

	typedef std::chrono::nanoseconds::rep Time;
	typedef std::chrono::nanoseconds Nanoseconds;
	typedef typename std::make_signed<Time>::type Skew;

	static Time current_time_nano() {
		using namespace std::chrono;
		typedef std::chrono::steady_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
	}

	static std::chrono::nanoseconds current_time() {
		using namespace std::chrono;
		typedef std::chrono::steady_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch());
	}

	/// Skew between system clock and steady clock.
	static Skew clock_skew() {
		using namespace std::chrono;
		Skew t0 = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
		Skew t1 = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
		return t1-t0;
	}

	static Time to_factory_time(Time system_time) {
		return system_time - clock_skew();
	}

}
