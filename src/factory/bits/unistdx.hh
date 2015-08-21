namespace factory {
	
	namespace bits {

		template<class T>
		std::string
		to_string(T rhs) {
			std::stringstream s;
			s << rhs;
			return s.str();
		}

		struct To_string {

			template<class T>
			inline
			To_string(T rhs):
				_s(to_string(rhs)) {}

			const char*
			c_str() const noexcept {
				return _s.c_str();
			}

		private:
			std::string _s;
		};

		typedef struct ::sigaction sigaction_type;

		struct Action: public sigaction_type {
			inline
			Action(void (*func)(int)) noexcept {
				this->sa_handler = func;
			}
		};

		struct object_tag {};
		struct pointer_tag {};
		struct smart_pointer_tag {};

		template<class tag>
		struct handler_wrapper {};

		template<>
		struct handler_wrapper<pointer_tag> {
			template<class T>
			static inline bool
			dirty(T obj) { return obj->dirty(); }
			template<class T, class E>
			static inline void
			handle(T obj, E ev) { obj->operator()(ev); }
		};

		template<>
		struct handler_wrapper<object_tag> {
			template<class T>
			static inline bool
			dirty(T& obj) { return obj.dirty(); }
			template<class T, class E>
			static inline void
			handle(T& obj, E ev) { obj.operator()(ev); }
		};

		template<>
		struct handler_wrapper<smart_pointer_tag> {
			template<class T>
			static inline bool
			dirty(T& obj) { return obj->dirty(); }
			template<class T, class E>
			static inline void
			handle(T& obj, E ev) { obj->operator()(ev); }
		};

		stdx::spin_mutex __forkmutex;

		int
		safe_fork() {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			return ::fork();
		}
	
	}

}
