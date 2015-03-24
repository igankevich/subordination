namespace factory {

	struct Null {};

	namespace components {

		void factory_stop();

		template<class K>
		void factory_send(K*);

		void factory_server_addr(std::ostream&);
	}

	typedef std::int64_t Time;

	Time now() {
		return std::chrono::steady_clock::now().time_since_epoch().count();
	}

}
