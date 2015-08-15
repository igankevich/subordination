namespace factory {

	namespace this_process {
	
		inline pid_type
		id() noexcept { return ::getpid(); }

		inline pid_type
		parent_id() noexcept { return ::getppid(); }
	
		template<class T>
		T getenv(const char* key, T dflt) {
			const char* text = ::getenv(key);
			if (!text) { return dflt; }
			std::stringstream s;
			s << text;
			T val;
			return (s >> val) ? val : dflt;
		}

		// TODO: setenv() is known to cause memory leaks
		template<class T>
		void env(const char* key, T val) {
			std::string str = bits::to_string(val);
			components::check(::setenv(key, str.c_str(), 1),
				__FILE__, __LINE__, __func__);
		}

		template<class ... Args>
		int execute(const Args& ... args) {
			bits::To_string tmp[] = { args... };
			const size_t argc = sizeof...(Args);
			char* argv[argc + 1];
			for (size_t i=0; i<argc; ++i) {
				argv[i] = const_cast<char*>(tmp[i].c_str());
			}
			argv[argc] = 0;
			return components::check(::execv(argv[0], argv), 
				__FILE__, __LINE__, __func__);
		}

		pid_type
		wait(int* status) {
			using components::check_if_not;
			return check_if_not<std::errc::interrupted>(
				::wait(status),
				__FILE__, __LINE__, __func__);
		}

		inline void
		bind_signal(int signum, const bits::Action& action) {
			components::check(::sigaction(signum, &action, 0),
				__FILE__, __LINE__, __func__);
		}

		const char*
		tempdir_path() {
			return "/tmp";
		}

	}

}
