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

		/// process-wide shared memory lock
		/// which does not require global
		/// variable
		struct global_lock {

			typedef stdx::spin_mutex mutex_type;

			global_lock() { lock(); }
			~global_lock() { unlock(); }

		private:

			void
			lock() {
				_shm = getshm();
				if (_shm == -1) {
					std::atexit(rm_shm_at_exit);
					_shm = getshm(IPC_CREAT);
					void* ptr = ::shmat(_shm, 0, 0);
					_mutex = new (ptr) mutex_type;
				} else {
					void* ptr = ::shmat(_shm, 0, 0);
					_mutex = static_cast<mutex_type*>(ptr);
				}
				_mutex->lock();
			}

			void
			unlock() {
				_mutex->unlock();
				::shmdt(_mutex);
			}

			static int
			getshm(int option=0) {
				return ::shmget(LOCK_KEY, sizeof(mutex_type), 0600 | option);
			}

			static void
			rm_shm_at_exit() {
				int shm = getshm();
				if (shm != -1) {
					::shmctl(shm, IPC_RMID, 0);
				}
			}

			int _shm;
			mutex_type* _mutex = nullptr;
			
			static constexpr ::key_t LOCK_KEY = 123;
		};

		stdx::spin_mutex __forkmutex;

		int
		safe_fork() {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			return ::fork();
		}

		inline void
		set_mandatory_flags(int fd) {
			fcntl(fd, F_SETFD, O_CLOEXEC);
			fcntl(fd, F_SETFL, O_NONBLOCK);
		}

		int
		safe_pipe(int fds[2]) {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			int ret = ::pipe(fds);
			if (ret != -1) {
				set_mandatory_flags(fds[0]);
				set_mandatory_flags(fds[1]);
				fcntl(fds[1], F_SETNOSIGPIPE, 1);
			}
			return ret;
		}

		int
		safe_open(const char* path, int oflag, int mode) {
			std::lock_guard<stdx::spin_mutex> lock(__forkmutex);
			int ret = ::open(path, oflag, mode);
			if (ret != -1) {
				set_mandatory_flags(ret);
			}
			return ret;
		}
	
	}

}
