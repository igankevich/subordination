namespace factory {

	struct Null {};
	struct Endpoint;

	namespace components {

		void factory_stop();

		template<class K>
		void factory_send(K*);
	}

	typedef std::int64_t Time;

	Time now() {
		struct timespec tm;
		::clock_gettime(CLOCK_MONOTONIC, &tm);
		return Time(tm.tv_sec)*Time(INTMAX_C(1000000000)) + Time(tm.tv_nsec);
	}

#ifdef LOG_USER_EVENTS

	void log_user_event(const char* name) {
		std::stringstream tmp;
		tmp << now() << ',' << name << '\n';
		std::cout << tmp.str();
	}
#else
#define log_user_event(unused) {}
#endif

}
