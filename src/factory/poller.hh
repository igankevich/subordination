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
			constexpr bool bad_event() const { return (this->events() & (Hup | Err)) != 0; }

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
			Event& operator=(const Event&) = default;

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

			enum note_type: char { Notify = '!' };
			typedef std::vector<Event> events_type;

			Poller() = default;
		
			void notify(note_type c = Notify) {
				check("write()", ::write(this->_mgmt_pipe.write_end(), &c, 
					sizeof(std::underlying_type<note_type>::type)));
			}
		
			template<class Callback>
			void run(Callback callback) {
				this->add(Event(Event::In, this->_mgmt_pipe.read_end()));
				while (!this->stopped()) this->wait(callback);
			}

			int pipe() const { return this->_mgmt_pipe.read_end(); }
			bool stopped() const { return this->_stopped; }

			void stop() {
				Logger<Level::COMPONENT>() << "Poller::stop()" << std::endl;
				this->_stopped = true;
			}
		
			void add(Event rhs) {
				Logger<Level::COMPONENT>() << "Poller::add " << rhs << std::endl;
				this->_events.push_back(rhs);
			}

			Event* operator[](int fd) {
				events_type::iterator pos = this->find(Event(fd)); 
				return pos == this->_events.end() ? nullptr : &*pos;
			}

			void ignore(int fd) {
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
					[this,&callback] (Event& e) {
						this->check_pollrdhup(e);
						if (e.events() != 0) {
							if (e.fd() == _mgmt_pipe.read_end()) {
								if (e.is_reading()) {
									this->consume_notify();
									callback(e);
								}
							} else {
								callback(e);
							}
						}
					}
				);
				this->remove_fds_if(std::mem_fn(&Event::bad_event));
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
		
			struct Pipe {

				Pipe() {
					check(::pipe(this->_fds), __FILE__, __LINE__, __func__);
					int flags = check(::fcntl(this->read_end(), F_GETFL), __FILE__, __LINE__, __func__);
					check(::fcntl(this->read_end(), F_SETFL, flags | O_NONBLOCK), __FILE__, __LINE__, __func__);
					check(::fcntl(this->read_end(), F_SETFD, FD_CLOEXEC), __FILE__, __LINE__, __func__);
				}

				~Pipe() {
					check(::close(this->_fds[0]), __FILE__, __LINE__, __func__);
					check(::close(this->_fds[1]), __FILE__, __LINE__, __func__);
				}

				int read_end() const { return this->_fds[0]; }
				int write_end() const { return this->_fds[1]; }

			private:
				int _fds[2];
			} _mgmt_pipe;
		
			events_type _events;
			bool _stopped = false;
		};

	}

}
