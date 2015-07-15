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
			constexpr Event(fd_type f, legacy_event ev, legacy_event rev):
				Basic_event{f,ev,rev} {}

			constexpr legacy_event revents() const { return this->Basic_event::revents; }

			void disable() { this->Basic_event::fd = -1; }
			constexpr fd_type fd() const { return this->Basic_event::fd; }
			constexpr bool bad_fd() const { return this->fd() < 0; } 

			constexpr bool in() const { return (this->revents() & In) != 0; }
			constexpr bool out() const { return (this->revents() & Out) != 0; }
			constexpr bool hup() const { return (this->revents() & Hup) != 0; }
			constexpr bool err() const { return (this->revents() & Err) != 0; }
			constexpr bool bad() const { return (this->revents() & (Hup | Err)) != 0; }

			void setev(event_type rhs) { this->Basic_event::events |= rhs; }
			void unsetev(event_type rhs) { this->Basic_event::events &= ~rhs; }
			void setrev(event_type rhs) { this->Basic_event::revents |= rhs; }
			void unsetrev(event_type rhs) { this->Basic_event::revents &= ~rhs; }
			void setall(event_type rhs) { this->setev(rhs); this->setrev(rhs); }

			ssize_t probe() const {
				char c;
				return ::recv(this->fd(), &c, 1, MSG_PEEK);
			}

			constexpr bool operator==(const Event& rhs) const { return this->fd() == rhs.fd(); }
			constexpr bool operator!=(const Event& rhs) const { return this->fd() != rhs.fd(); }
			constexpr bool operator< (const Event& rhs) const { return this->fd() <  rhs.fd(); }
			Event& operator=(const Event&) = default;

			friend std::ostream& operator<<(std::ostream& out, const Event& rhs) {
				return out << "{fd=" << rhs.fd() << ",ev="
					<< (rhs.in() ? 'r' : '-')
					<< (rhs.out() ? 'w' : '-')
					<< (rhs.hup() ? 'c' : '-')
					<< (rhs.err() ? 'e' : '-')
					<< '}';
			}

		};

		static_assert(sizeof(Event) == sizeof(Basic_event),
			"The size of Event does not match the size of ``struct pollfd''.");

		struct Pipe {

			Pipe() {
				check(::pipe(this->_fds), __FILE__, __LINE__, __func__);
				int flags = check(::fcntl(this->read_end(), F_GETFL), __FILE__, __LINE__, __func__);
				check(::fcntl(this->read_end(), F_SETFL, flags | O_NONBLOCK), __FILE__, __LINE__, __func__);
				check(::fcntl(this->read_end(), F_SETFD, FD_CLOEXEC), __FILE__, __LINE__, __func__);
			}

			Pipe(Pipe&& rhs): _fds{rhs._fds[0], rhs._fds[1]} {
				rhs._fds[0] = -1;
				rhs._fds[1] = -1;
			}

			~Pipe() {
				if (this->_fds[0] >= 0) {
					check(::close(this->_fds[0]), __FILE__, __LINE__, __func__);
				}
				if (this->_fds[1] >= 0) {
					check(::close(this->_fds[1]), __FILE__, __LINE__, __func__);
				}
			}

			int read_end() const { return this->_fds[0]; }
			int write_end() const { return this->_fds[1]; }

		private:
			int _fds[2];
		};
		
		struct Poller {

			enum note_type: char { Notify = '!' };
			typedef std::vector<Event> events_type;
			typedef Pipe pipe_type;
			typedef int fd_type;

			Poller() = default;
			~Poller() = default;
			Poller(const Poller&) = delete;
			Poller& operator=(const Poller&) = delete;
			Poller(Poller&& rhs):
				_pipe(std::move(rhs._pipe)),
				_events(std::move(rhs._events)),
				_stopped(rhs._stopped)
				{}
		
			void notify(note_type c = Notify) {
				check("write()", ::write(this->_pipe.write_end(), &c, 
					sizeof(std::underlying_type<note_type>::type)));
			}
		
			template<class Callback>
			void run(Callback callback) {
				this->add(Event(Event::In, this->_pipe.read_end()));
				while (!this->stopped()) this->wait(callback);
			}

			fd_type pipe() const { return this->_pipe.read_end(); }
			bool stopped() const { return this->_stopped; }

			void stop() {
				Logger<Level::COMPONENT>() << "Poller::stop()" << std::endl;
				this->_stopped = true;
			}
		
			void add(Event rhs) {
				Logger<Level::COMPONENT>() << "Poller::add " << rhs << std::endl;
				this->_events.push_back(rhs);
			}

			Event* operator[](fd_type fd) {
				events_type::iterator pos = this->find(Event(fd)); 
				return pos == this->_events.end() ? nullptr : &*pos;
			}

			void disable(fd_type fd) {
				events_type::iterator pos = this->find(Event(fd)); 
				if (pos != this->_events.end()) {
					Logger<Level::COMPONENT>() << "ignoring fd=" << pos->fd() << std::endl;
					pos->disable();
				}
			}

		private:

			events_type::iterator find(const Event& ev) {
				return std::find(this->_events.begin(),
					this->_events.end(), ev);
			}

			void consume_notify() {
				const size_t n = 20;
				char tmp[n];
				ssize_t c;
				while ((c = ::read(this->pipe(), tmp, n)) != -1);
			}

			static int check_poll(const char* func, int ret) {
				return (errno == EINTR) ? ret : check(func, ret);
			}

			template<class Pred>
			void remove_fds_if(Pred pred) {
				events_type::iterator fixed_end = this->_events.end();
				events_type::iterator it = std::remove_if(this->_events.begin(),
					fixed_end, pred);
				this->_events.erase(it, fixed_end);
			}

			void check_pollrdhup(Event& e) {
			#if !HAVE_DECL_POLLRDHUP
				if (e.probe() == 0) {
					e.setrev(Event::Hup);
				}
			#endif
			}
		
			template<class Callback>
			void wait(Callback callback) {

				do {
					Logger<Level::COMPONENT>() << "poll(): size="
						<< this->_events.size() << std::endl;
					check_poll("poll()", ::poll(this->_events.data(),
						this->_events.size(), -1));
				} while (errno == EINTR);

				this->remove_fds_if(std::mem_fn(&Event::bad_fd));

				std::for_each(this->_events.begin(), this->_events.end(),
					[this,&callback] (Event& ev) {
						this->check_pollrdhup(ev);
						if (ev.revents() != 0) {
							if (ev.fd() == _pipe.read_end()) {
								if (ev.in()) {
									this->consume_notify();
									callback(ev);
								}
							} else {
								callback(ev);
							}
						}
					}
				);
				this->remove_fds_if(std::mem_fn(&Event::bad));
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
		
			pipe_type _pipe;
			events_type _events;
			bool _stopped = false;
		};

	}

}
