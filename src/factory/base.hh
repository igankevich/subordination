namespace factory {

	struct Null {};

	namespace components {

		void factory_stop();

		template<class K>
		void factory_send(K*);

		void factory_server_addr(std::ostream&);
	}

//	typedef std::int64_t Time;
//
//	Time now() {
//		return std::chrono::steady_clock::now().time_since_epoch().count();
//	}

	typedef std::chrono::nanoseconds::rep Time;
	typedef std::chrono::nanoseconds Nanoseconds;

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


}
