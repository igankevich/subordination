namespace factory {

	namespace components {

		typedef struct ::pollfd Basic_event;

		struct Event: public Basic_event {

			typedef decltype(Basic_event::events) legacy_event;
			typedef int fd_type;

			enum event_type: legacy_event {
				In = POLLIN,
				Out = POLLOUT,
				Hup = POLLHUP | POLLRDHUP,
				Err = POLLERR | POLLNVAL
			};

			friend
			constexpr legacy_event operator|(event_type lhs, event_type rhs) {
				return static_cast<legacy_event>(lhs) | static_cast<legacy_event>(rhs);
			}
			friend
			constexpr legacy_event operator&(legacy_event lhs, event_type rhs) {
				return lhs & static_cast<legacy_event>(rhs);
			}

			constexpr Event(): Basic_event{-1,0,0} {}
			constexpr explicit Event(fd_type f): Basic_event{f,0,0} {}
			constexpr Event(legacy_event e, fd_type f): Basic_event{f,e,0} {}

			constexpr legacy_event events() const { return this->Basic_event::revents; }
			void events(legacy_event rhs) { this->Basic_event::events = rhs; }

			void disable() { this->Basic_event::fd = -1; }
			constexpr fd_type fd() const { return this->Basic_event::fd; }
			constexpr bool bad_fd() const { return this->fd() < 0; } 

			constexpr bool is_reading() const { return (this->events() & In) != 0; }
			constexpr bool is_writing() const { return (this->events() & Out) != 0; }
			constexpr bool is_closing() const { return (this->events() & Hup) != 0; }
			constexpr bool is_error() const { return (this->events() & Err) != 0; }

			// TODO: replace with set/unset ev/rev
			void no_reading() { this->Basic_event::revents &= ~In; }
			void writing() {
				this->Basic_event::events |= Out;
				this->Basic_event::revents |= Out;
			}
			void reading() {
				this->Basic_event::events |= In;
				this->Basic_event::revents |= In;
			}

			void setev(event_type rhs) { this->Basic_event::events |= rhs; }
			void unsetev(event_type rhs) { this->Basic_event::events &= ~rhs; }
			void setrev(event_type rhs) { this->Basic_event::revents |= rhs; }
			void unsetrev(event_type rhs) { this->Basic_event::revents &= ~rhs; }

			ssize_t probe() const {
				char c;
				return ::recv(this->fd(), &c, 1, MSG_PEEK);
			}

			constexpr bool operator==(const Event& rhs) const { return this->fd() == rhs.fd(); }
			constexpr bool operator!=(const Event& rhs) const { return this->fd() != rhs.fd(); }
			constexpr bool operator< (const Event& rhs) const { return this->fd() <  rhs.fd(); }

			friend std::ostream& operator<<(std::ostream& out, const Event& rhs) {
				return out << "{fd=" << rhs.fd() << ",ev="
					<< (rhs.is_reading() ? 'r' : '-')
					<< (rhs.is_writing() ? 'w' : '-')
					<< (rhs.is_closing() ? 'c' : '-')
					<< (rhs.is_error() ? 'e' : '-')
					<< '}';
			}

		};

		static_assert(sizeof(Event) == sizeof(Basic_event),
			"The size of Event does not match the size of ``struct pollfd''.");
		
		struct Poller {

			enum struct State {
				DEFAULT,
				STOPPING,
				STOPPED
			};

			Poller():
				_state(State::DEFAULT),
				_mgmt_pipe(),
				_events()
			{}
		
			int notification_pipe() const { return _mgmt_pipe.read_end(); }

			void notify() {
				char c = NOTIFY_SYMBOL;
				check("Poller::notify()", ::write(_mgmt_pipe.write_end(), &c, 1));
			}

			void notify_stopping() {
				char c = STOP_SYMBOL;
				check("Poller::notify_stopping()", ::write(_mgmt_pipe.write_end(), &c, 1));
			}

		
			template<class Callback>
			void run(Callback callback) {
				add(Event(Event::In, _mgmt_pipe.read_end()));
				while (!stopped()) this->wait(callback);
			}

			bool stopped() const { return _state == State::STOPPED; }
			bool stopping() const { return _state == State::STOPPING; }

			void stop() {
				Logger<Level::COMPONENT>() << "Poller::stop()" << std::endl;
				::close(_mgmt_pipe.write_end());
			}
		
			void add(Event rhs) {
				Logger<Level::COMPONENT>() << "Poller::add(" << rhs.fd() << ", " << rhs << ")" << std::endl;
				_events.push_back(rhs);
			}

			Event* operator[](int fd) {
				auto pos = std::find(_events.begin(), _events.end(), Event(fd));
				return pos == _events.end() ? nullptr : &*pos;
			}

			void ignore(int fd) {
				auto pos = std::find(_events.begin(), _events.end(), Event(fd));
				if (pos != _events.end()) {
					Logger<Level::COMPONENT>() << "ignoring fd=" << pos->fd() << std::endl;
					pos->disable();
				}
			}

		private:

			void consume_notify() {
				const size_t n = 20;
				char tmp[n];
				ssize_t c;
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
					Logger<Level::COMPONENT>() << "poll()" << std::endl;
					check_poll("poll()", ::poll(_events.data(), _events.size(), -1));
				} while (errno == EINTR);

				size_t nfds = _events.size();
				for (size_t n=0; n<nfds; ++n) {
					Event e = _events[n];
					if (e.bad_fd()) {
						erase(e);
						--n;
						--nfds;
						continue;
					}
				#if !HAVE_DECL_POLLRDHUP
					if (e.probe() == 0) {
						e.setrev(Event::Hup);
					}
				#endif
					if (e.events() == 0) continue;
					if (e.fd() == _mgmt_pipe.read_end()) {
						if (e.is_closing()) {
							Logger<Level::COMPONENT>() << "Stopping poller" << std::endl;
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
				std::ostream::sentry s(out);
				if (s) {
					out << '{';
					intersperse_iterator<Event> it(out, ",");
					std::copy(rhs._events.cbegin(), rhs._events.cend(), it);
					out << '}';
				}
				return out;
			}
		
			State _state = State::DEFAULT;

			struct Pipe {

				Pipe() {
					check("pipe()", ::pipe(this->_fds));
					int flags = ::fcntl(this->read_end(), F_GETFL);
					::fcntl(this->read_end(), F_SETFL, flags | O_NONBLOCK);
					::fcntl(this->read_end(), F_SETFD, FD_CLOEXEC);
				}

				~Pipe() {
					::close(this->_fds[0]);
					::close(this->_fds[1]);
				}

				int read_end() const { return this->_fds[0]; }
				int write_end() const { return this->_fds[1]; }

			private:
				int _fds[2];
			} _mgmt_pipe;
		
			std::vector<Event> _events;

			static const char STOP_SYMBOL   = 's';
			static const char NOTIFY_SYMBOL = '!';
		};

	}

}
