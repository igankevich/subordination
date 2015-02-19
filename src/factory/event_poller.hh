namespace factory {

	typedef struct ::epoll_event Basic_event;

	template<class Data>
	struct Event: public Basic_event {

		Event() { user_data(nullptr); }
		Event(int ev, Data* ptr)  {
			Basic_event::events = ev;
			user_data(ptr);
		}
//		~Event() { delete_user_data(); }

		int events() const { return Basic_event::events; }
		void events(int evs) { Basic_event::events = evs; }
		int fd() const { return user_data()->fd(); }

		void raw_fd(int fd) { Basic_event::data.fd = fd; }
		int raw_fd() const { return Basic_event::data.fd; }

		void delete_user_data() {
			if (user_data() != nullptr) {
				delete user_data();
				user_data(nullptr);
			}
		}

		bool is_closing() const { return (events() & EPOLLRDHUP) != 0; }
		bool is_reading() const { return (events() & EPOLLIN) != 0; }
		bool is_writing() const { return (events() & EPOLLOUT) != 0; }

		Data* user_data() { return static_cast<Data*>(Basic_event::data.ptr); }
		const Data* user_data() const { return static_cast<Data*>(Basic_event::data.ptr); }

		friend std::ostream& operator<<(std::ostream& out, const Event<Data>& rhs) {
			return out
				<< (rhs.is_reading() ? 'r' : ' ')
				<< (rhs.is_writing() ? 'w' : ' ')
				<< (rhs.is_closing() ? 'c' : ' ');

		}

	private:
		void user_data(Data* d) { Basic_event::data.ptr = d; }
	};

	
	template<class User_data>
	struct Event_poller {

		typedef Event<User_data> E;

		static_assert(sizeof(E) == sizeof(Basic_event), "The size of Event does not match the size of epoll_event.");

		Event_poller():
			_epollfd(check("epoll_create()", ::epoll_create(1))),
			_stopped(false),
			_mgmt_pipe()
		{
			E ev(EPOLLIN | EPOLLET | EPOLLRDHUP, nullptr);
			ev.raw_fd(_mgmt_pipe.read_end());
			check("epoll_ctl()", ::epoll_ctl(_epollfd, EPOLL_CTL_ADD, ev.raw_fd(), &ev));
		}
	
		~Event_poller() {
			::close(_epollfd);
			std::for_each(_all_events.begin(), _all_events.end(), [] (std::pair<int,E> pair) {
				pair.second.delete_user_data();
			});
		}
	
		template<class Callback>
		void run(Callback callback) {
			while (!stopped()) this->wait(callback);
		}

		bool stopped() const { return _stopped; }

		void stop() {
			char s = 's';
			::write(_mgmt_pipe.write_end(), &s, 1);
		}
	
		void register_socket(E ev) {
			std::clog << "Event_poller::register_socket(" << ev.fd() << ", " << ev << ")" << std::endl;
			check("epoll_ctl()", ::epoll_ctl(_epollfd, EPOLL_CTL_ADD, ev.fd(), &ev));
			_all_events[ev.fd()] = ev;
		}

		void erase(E ev) {
			check("epoll_ctl()", ::epoll_ctl(_epollfd, EPOLL_CTL_DEL, ev.fd(), &ev));
			_all_events.erase(ev.fd());
			ev.delete_user_data();
		}

		void modify_socket(E ev) {
			check("epoll_ctl()", ::epoll_ctl(_epollfd, EPOLL_CTL_MOD, ev.fd(), &ev));
			_all_events[ev.fd()] = ev;
		}

	private:
	
		template<class Callback>
		void wait(Callback callback) {
			int nfds;
			do {
//				nfds = check("epoll_wait()", ::epoll_wait(_epollfd, _events, MAX_EVENTS, -1));
				nfds = ::epoll_wait(_epollfd, _events, MAX_EVENTS, -1);
			} while (nfds < 0 && errno == EINTR);
			for (int n=0; n<nfds; ++n) {
				if (_events[n].raw_fd() == _mgmt_pipe.read_end()) {
					_stopped = true;
					if (_events[n].is_closing()) {
						check("epoll_ctl()", ::epoll_ctl(_epollfd, EPOLL_CTL_DEL, _events[n].raw_fd(), &_events[n]));
					}
				} else {
					callback(_events[n]);
					if (_events[n].is_closing()) {
						erase(_events[n]);
					}
				}
			}
		}
	
		int _epollfd;
		bool _stopped;

		union Pipe {

			Pipe() {
				::pipe(_fds);
				int flags = ::fcntl(read_end(), F_GETFL);
				::fcntl(read_end(), F_SETFL, flags | O_NONBLOCK);
			}

			~Pipe() {
				::close(_fds[0]);
				::close(_fds[1]);
			}

			int read_end() const { return _fds[0]; }
			int write_end() const { return _fds[1]; }

		private:
			struct {
				int _read_fd;
				int _write_fd;
			} _unused;
			int _fds[2];

		} _mgmt_pipe;
	
		static const int MAX_EVENTS = 128;
		E _events[MAX_EVENTS];
		std::unordered_map<int, E> _all_events;
	};

}
