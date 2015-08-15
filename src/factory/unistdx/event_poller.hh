namespace factory {

	namespace unix {

		typedef struct ::pollfd Basic_event;

		struct poll_event: public Basic_event {

			typedef decltype(Basic_event::events) legacy_event;
			typedef int fd_type;

			enum event_type: legacy_event {
				In = POLLIN,
				Out = POLLOUT,
				Hup = POLLHUP | POLLRDHUP,
				Err = POLLERR | POLLNVAL,
				Inout = POLLIN | POLLOUT,
				Def = POLLRDHUP
			};

			friend constexpr legacy_event
			operator|(event_type lhs, event_type rhs) noexcept {
				return static_cast<legacy_event>(lhs) | static_cast<legacy_event>(rhs);
			}

			friend constexpr legacy_event
			operator|(legacy_event lhs, event_type rhs) noexcept {
				return lhs | static_cast<legacy_event>(rhs);
			}

			friend constexpr legacy_event
			operator|(event_type lhs, legacy_event rhs) noexcept {
				return static_cast<legacy_event>(lhs) | rhs;
			}

			friend constexpr legacy_event
			operator&(legacy_event lhs, event_type rhs) noexcept {
				return lhs & static_cast<legacy_event>(rhs);
			}

			friend constexpr legacy_event
			operator&(event_type lhs, legacy_event rhs) noexcept {
				return static_cast<legacy_event>(lhs) & rhs;
			}

			explicit constexpr
			poll_event(fd_type f=-1, legacy_event ev=0, legacy_event rev=0) noexcept:
				Basic_event{f,ev|Def,rev} {}

			constexpr legacy_event
			revents() const noexcept {
				return this->Basic_event::revents;
			}

			inline void
			disable() noexcept {
				this->Basic_event::fd = -1;
			}

			constexpr fd_type
			fd() const noexcept {
				return this->Basic_event::fd;
			}
			
			constexpr bool
			bad_fd() const noexcept {
				return this->fd() < 0;
			} 

			constexpr bool
			in() const noexcept {
				return (this->revents() & In) != 0;
			}

			constexpr bool
			out() const noexcept {
				return (this->revents() & Out) != 0;
			}

			constexpr bool
			hup() const noexcept {
				return (this->revents() & Hup) != 0;
			}

			constexpr bool
			err() const noexcept {
				return (this->revents() & Err) != 0;
			}

			constexpr bool
			bad() const noexcept {
				return (this->revents() & (Hup | Err)) != 0;
			}

			inline void
			setev(event_type rhs) noexcept {
				this->Basic_event::events |= rhs;
			}

			inline void
			unsetev(event_type rhs) noexcept {
				this->Basic_event::events &= ~rhs;
			}

			inline void
			setrev(event_type rhs) noexcept {
				this->Basic_event::revents |= rhs;
			}

			inline void
			unsetrev(event_type rhs) noexcept {
				this->Basic_event::revents &= ~rhs;
			}

			inline void
			setall(event_type rhs) noexcept {
				this->setev(rhs); this->setrev(rhs);
			}

			inline ssize_t
			probe() const noexcept {
				char c;
				return ::recv(this->fd(), &c, 1, MSG_PEEK);
			}

			constexpr bool
			operator==(const poll_event& rhs) const noexcept {
				return this->fd() == rhs.fd();
			}

			constexpr bool
			operator!=(const poll_event& rhs) const noexcept {
				return this->fd() != rhs.fd();
			}

			constexpr bool
			operator<(const poll_event& rhs) const noexcept {
				return this->fd() < rhs.fd();
			}

			inline
			poll_event& operator=(const poll_event&) noexcept = default;

			friend std::ostream&
			operator<<(std::ostream& out, const poll_event& rhs) {
				return out << "{fd=" << rhs.fd() << ",ev="
					<< (rhs.in() ? 'r' : '-')
					<< (rhs.out() ? 'w' : '-')
					<< (rhs.hup() ? 'c' : '-')
					<< (rhs.err() ? 'e' : '-')
					<< '}';
			}

		};

		static_assert(sizeof(poll_event) == sizeof(Basic_event),
			"The size of poll_event does not match the size of ``struct pollfd''.");

		template<class H, class D=std::default_delete<H>>
		struct event_poller {

			enum note_type: char { Notify = '!' };
			enum timeout_type: int { Infinite = -1 };
			typedef int fd_type;
			typedef poll_event event_type;
			typedef std::unique_ptr<H,D> handler_type;
			typedef std::vector<event_type> events_type;
			typedef std::vector<handler_type> handlers_type;
			typedef events_type::size_type size_type;
			typedef H* ptr_type;

			event_poller() {
				this->_pipe.in().setf(unix::fd::non_blocking | unix::fd::close_on_exec);
				this->add(poll_event(this->_pipe.in().get_fd(), poll_event::In), nullptr);
			}
			~event_poller() = default;
			event_poller(const event_poller&) = delete;
			event_poller& operator=(const event_poller&) = delete;
			event_poller(event_poller&& rhs):
				_pipe(std::move(rhs._pipe)),
				_events(std::move(rhs._events)),
				_handlers(std::move(rhs._handlers)),
				_stopped(rhs._stopped)
				{}
		
			void notify(note_type c = Notify) {
				check(::write(this->_pipe.out().get_fd(),
					&c, sizeof(note_type)),
					__FILE__, __LINE__, __func__);
			}
		
			template<class Callback>
			void run(Callback callback) {
				while (!this->stopped()) this->wait(callback);
			}

			fd_type pipe_in() const { return this->_pipe.in().get_fd(); }
			bool stopped() const { return this->_stopped; }

			void stop() {
				std::clog << "event_poller::stop()" << std::endl;
				this->_stopped = true;
			}

			void clear() {
				this->_events.clear();
				this->_handlers.clear();
			}

			void add(event_type ev, ptr_type ptr, D deleter = D()) {
				std::clog << "event_poller::add " << ev << std::endl;
				this->_events.push_back(ev);
				this->_handlers.emplace_back(std::move(handler_type(ptr, deleter)));
			}
		
			void disable(fd_type fd) {
				events_type::iterator pos = this->find(poll_event{fd}); 
				if (pos != this->_events.end()) {
					std::clog << "ignoring fd=" << pos->fd() << std::endl;
					pos->disable();
				}
			}

		private:

			events_type::iterator find(const poll_event& ev) {
				return std::find(this->_events.begin(),
					this->_events.end(), ev);
			}

			void consume_notify() {
				const size_t n = 20;
				char tmp[n];
				ssize_t c;
				while ((c = ::read(this->pipe_in(), tmp, n)) != -1);
			}

			template<class Pred>
			void remove_fds_if(Pred pred) {
				typedef events_type::iterator It1;
				typedef typename handlers_type::iterator It2;
				It1 fixed_beg = this->_events.begin();
				It1 fixed_end = this->_events.end();
				It2 fixed_beg2 = this->_handlers.begin();
				It2 fixed_end2 = this->_handlers.end();
//				typedef typename decltype(beg3)::reference reference;
				auto result = std::remove_if(stdx::make_paired(fixed_beg, fixed_beg2),
					stdx::make_paired(fixed_end, fixed_end2), stdx::apply_to<0>(pred));
//				auto result = std::remove_if(beg3, end3,
//					[pred] (reference rhs) {
//						return pred(std::get<0>(rhs));
//					}
//				);
				this->_events.erase(result.first(), fixed_end);
				this->_handlers.erase(result.second(), fixed_end2);

//				std::clog
//					<< "all events when erasing: "
//					<< *this << std::endl;
//				size_type i = 0;
//				It2 it2 = std::remove_if(fixed_beg2, fixed_end2,
//					[pred,this,&i](handler_type& h) {
//						bool ret =  pred(this->_events[i]);
//						++i;
//						return ret;
//					}
//				);
//				It1 it = std::remove_if(fixed_beg, fixed_end, pred);
//				std::for_each(it, fixed_end, [] (event_type& ev) {
//					std::clog
//						<< "erasing event "
//						<< ev << std::endl;
//				});
//				std::for_each(it2, fixed_end2, [] (const handler_type& ptr) {
//					if (ptr) {
//						std::clog
//							<< "erasing server "
//							<< *ptr << std::endl;
//					}
//				});
//				this->_events.erase(it, fixed_end);
//				this->_handlers.erase(it2, fixed_end2);
			}

			void check_pollrdhup(poll_event& e) {
				#if !HAVE_DECL_POLLRDHUP
				if (e.probe() == 0) {
					e.setrev(poll_event::Hup);
				}
				#endif
			}

			void check_dirty() {
				size_type i = 0;
				std::for_each(this->_events.begin(), this->_events.end(),
					[this,&i] (poll_event& ev) {
						handler_type& h = this->_handlers[i];
						if (h && h->dirty()) {
							ev.setev(poll_event::Out);
						}
						++i;
					}
				);
			}
		
			template<class Callback>
			void wait(Callback callback) {

				do {
					check_dirty();
					std::clog << "poll(): size="
						<< this->_events.size() << std::endl;
					check_if_not<std::errc::interrupted>(::poll(this->_events.data(),
						this->_events.size(), Infinite),
						__FILE__, __LINE__, __func__);
				} while (errno == EINTR);

				this->remove_fds_if(std::mem_fn(&poll_event::bad_fd));

				size_type i = 0;
				std::for_each(this->_events.begin(), this->_events.end(),
					[this,&callback,&i] (poll_event& ev) {
						handler_type& h = this->_handlers[i];
						if (h && h->dirty()) {
							ev.setrev(poll_event::Out);
						}
						this->check_pollrdhup(ev);
						if (!ev.bad_fd()) {
							if (ev.fd() == this->_pipe.in()) {
								if (ev.in()) {
									this->consume_notify();
									callback(ev);
								}
							} else {
								if (h) {
									h->operator()(ev);
								} else {
									callback(ev);
								}
							}
						}
						++i;
					}
				);
				this->remove_fds_if(std::mem_fn(&poll_event::bad));
			}

			friend std::ostream& operator<<(std::ostream& out, const event_poller& rhs) {
				std::ostream::sentry s(out);
				if (s) {
					out << '{';
					stdx::intersperse_iterator<poll_event> it(out, ",");
					std::copy(rhs._events.cbegin(), rhs._events.cend(), it);
					out << '}';
				}
				return out;
			}
		
			unix::pipe _pipe;
			events_type _events;
			handlers_type _handlers;
			volatile bool _stopped = false;
		};

	}

}
