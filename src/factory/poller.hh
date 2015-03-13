namespace factory {

	typedef struct ::pollfd Basic_event;

	struct Event: public Basic_event {

		Event()  { fd(-1); events(0); Basic_event::revents = 0; }
		explicit Event(int f)  { fd(f); }
		Event(int e, int f)  { fd(f); events(e); }

		int events() const { return Basic_event::revents; }
		void events(int rhs) { Basic_event::events = rhs; }

		void fd(int rhs) { Basic_event::fd = rhs; }
		int fd() const { return Basic_event::fd; }
		bool bad_fd() const { return fd() <= 0; } 

		bool is_reading() const { return (events() & POLLIN) != 0; }
		bool is_writing() const { return (events() & POLLOUT) != 0; }
		bool is_closing() const {
			return (events() & POLLRDHUP) != 0
				|| (events() & POLLHUP) != 0;
		}
		bool is_error() const {
			return (events() & POLLERR) != 0
				|| (events() & POLLNVAL) != 0;
		}

		void no_reading() { Basic_event::revents &= ~POLLIN; }
		void writing() {
			Basic_event::events |= POLLOUT;
			Basic_event::revents |= POLLOUT;
		}
		void reading() {
			Basic_event::events |= POLLIN;
			Basic_event::revents |= POLLIN;
		}

		bool operator==(const Event& rhs) const { return fd() == rhs.fd(); }
		bool operator!=(const Event& rhs) const { return fd() != rhs.fd(); }
		bool operator< (const Event& rhs) const { return fd() <  rhs.fd(); }

		friend std::ostream& operator<<(std::ostream& out, const Event& rhs) {
			return out
				<< rhs.fd() << ' '
				<< (rhs.is_reading() ? 'r' : ' ')
				<< (rhs.is_writing() ? 'w' : ' ')
				<< (rhs.is_closing() ? 'c' : ' ')
				<< (rhs.is_error() ? 'e' : ' ');
		}

	};

	static_assert(sizeof(Event) == sizeof(Basic_event),
		"The size of Event does not match the size of Basic_event.");
	
	struct Poller {

		enum struct State: char {
			DEFAULT,
			STOPPING,
			STOPPED
		};

		Poller():
			_state(State::DEFAULT),
			_mgmt_pipe(),
			_events()
		{
			add(Event(POLLIN | POLLRDHUP, _mgmt_pipe.read_end()));
		}
	
		int notification_pipe() const { return _mgmt_pipe.read_end(); }

		void notify() {
			char c = NOTIFY_SYMBOL;
			check("Poller::notify()", ::write(_mgmt_pipe.write_end(), &c, 1));
		}

		void notify_stopping() {
			char c = STOP_SYMBOL;
			::write(_mgmt_pipe.write_end(), &c, 1);
		}

	
		template<class Callback>
		void run(Callback callback) { while (!stopped()) this->wait(callback); }

		bool stopped() const { return _state == State::STOPPED; }
		bool stopping() const { return _state == State::STOPPING; }

		void stop() {
			Logger(Level::COMPONENT) << "Poller::stop()" << std::endl;
			::close(_mgmt_pipe.write_end());
		}
	
		void add(Event rhs) {
			Logger(Level::COMPONENT) << "Poller::add(" << rhs.fd() << ", " << rhs << ")" << std::endl;
			_events.push_back(rhs);
		}

		Event* operator[](int fd) {
			auto pos = std::find(_events.begin(), _events.end(), Event(fd));
			return pos == _events.end() ? nullptr : &*pos;
		}

		void ignore(int fd) {
			auto pos = std::find(_events.begin(), _events.end(), Event(fd));
			if (pos != _events.end()) {
				Logger(Level::COMPONENT) << "ignoring fd=" << pos->fd() << std::endl;
				pos->fd(-1);
			}
		}

	private:

		void consume_notify() {
			const size_t n = 20;
			char tmp[n];
			int c;
			while ((c = ::read(_mgmt_pipe.read_end(), tmp, n)) != -1) {
				if (std::any_of(tmp, tmp + c, [this] (char rhs) { return rhs == STOP_SYMBOL; })) {
					_state = State::STOPPING;
				}
			}
		}

		void erase(Event rhs) {
			auto pos = std::find(_events.begin(), _events.end(), rhs);
			if (pos != _events.end()) {
				_events.erase(pos);
			}
		}

		void erase(int fd) { erase(Event(fd)); }


		static int check_poll(const char* func, int ret) {
			return (errno == EINTR) ? ret : check(func, ret);
		}
	
		template<class Callback>
		void wait(Callback callback) {

			do {
				Logger(Level::COMPONENT) << "poll()" << std::endl;
				check_poll("poll()", ::poll(_events.data(), _events.size(), -1));
			} while (errno == EINTR);

			int nfds = _events.size();
			for (int n=0; n<nfds; ++n) {
				Event e = _events[n];
				if (e.bad_fd()) {
					erase(e);
					--n;
					--nfds;
					continue;
				}
				if (e.events() == 0) continue;
				if (e.fd() == _mgmt_pipe.read_end()) {
					if (e.is_closing()) {
						Logger(Level::COMPONENT) << "Stopping poller" << std::endl;
						_state = State::STOPPED;
					}
					if (e.is_reading()) {
						consume_notify();
						callback(e);
					}
				} else {
					callback(e);
					if (e.is_closing() || e.is_error()) {
						erase(e);
						--n;
						--nfds;
					}
				}
			}
		}

		friend std::ostream& operator<<(std::ostream& out, const Poller& rhs) {
			std::ostream_iterator<Event> it(out, ", ");
			std::copy(rhs._events.cbegin(), rhs._events.cend(), it);
			return out;
		}
	
		State _state;

		union Pipe {

			Pipe() {
				::pipe(_fds);
				int flags = ::fcntl(read_end(), F_GETFL);
				::fcntl(read_end(), F_SETFL, flags | O_NONBLOCK | O_CLOEXEC);
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
	
		std::vector<Event> _events;

		static const char STOP_SYMBOL   = 's';
		static const char NOTIFY_SYMBOL = '!';
	};

}
