namespace factory {

	namespace components {

//		void print_all_endpoints(std::ostream&);
//		void stop_all_factories(bool now=false);
		template<class App, class Endp, class Buf>
		void forward_to_app(App app, const Endp& from, Buf& buf);

		template<class Ret>
		Ret
		check(Ret ret, Ret bad, const char* file, const int line, const char* func);

		template<class Ret>
		inline Ret
		check(Ret ret, const char* file, const int line, const char* func);

		template<std::errc ignored_errcode, class Ret>
		inline Ret
		check_if_not(Ret ret, const char* file, const int line, const char* func);

	}

}
