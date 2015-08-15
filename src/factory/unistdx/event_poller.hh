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

			explicit constexpr
			poll_event(const unix::fd& f, legacy_event ev=0, legacy_event rev=0) noexcept:
				poll_event(f.get_fd(),ev,rev) {}

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

			typedef std::unique_ptr<H,D> handler_type;
			typedef std::vector<poll_event> events_type;
			typedef std::vector<handler_type> handlers_type;
			typedef events_type::size_type size_type;
			typedef H* ptr_type;

			inline
			event_poller() {
				init_pipe();
			}

			~event_poller() = default;
			event_poller(const event_poller&) = delete;
			event_poller& operator=(const event_poller&) = delete;

			inline
			event_poller(event_poller&& rhs) noexcept:
				_pipe(std::move(rhs._pipe)),
				_events(std::move(rhs._events)),
				_handlers(std::move(rhs._handlers)),
				_stopped(rhs._stopped)
				{}
		
			inline void
			notify(note_type c = Notify) {
				this->_pipe.out().write(&c, sizeof(note_type));
			}
		
			template<class Callback>
			inline void
			run(Callback callback) {
				while (!this->stopped()) this->wait(callback);
			}

			inline fd_type
			pipe_in() const noexcept {
				return this->_pipe.in().get_fd();
			}

			inline bool
			stopped() const noexcept {
				return this->_stopped;
			}

			inline void
			stop() noexcept {
//				std::clog << "event_poller::stop()" << std::endl;
				this->_stopped = true;
			}

			inline void
			clear() noexcept {
				// TODO: do not delete first
				this->_events.erase(_events.begin() + NPIPES, _events.end());
				this->_handlers.clear();
			}

			void
			insert_special(poll_event ev) {
				std::clog << "event_poller::insert_special " << ev << std::endl;
				this->_events.insert(this->begin(), ev);
				++nspecial;
			}

			void emplace(poll_event ev, handler_type&& handler) {
				std::clog << "event_poller::emplace " << ev << std::endl;
				this->_events.push_back(ev);
				this->_handlers.emplace_back(std::move(handler));
			}

//			void add(poll_event ev, ptr_type ptr, D deleter = D()) {
//				std::clog << "event_poller::add " << ev << std::endl;
//				this->_events.push_back(ev);
//				this->_handlers.emplace_back(std::move(handler_type(ptr, deleter)));
//			}
		
			void disable(fd_type fd) {
				events_type::iterator pos = this->find(poll_event{fd}); 
				if (pos != this->_events.end()) {
					std::clog << "ignoring fd=" << pos->fd() << std::endl;
					pos->disable();
				}
			}

		private:

			inline void
			init_pipe() {
				this->_pipe.in().setf(unix::fd::non_blocking | unix::fd::close_on_exec);
				_events.insert(_events.begin(),
					poll_event{this->_pipe.in(), poll_event::In});
			}

			inline events_type::const_iterator
			begin() const noexcept {
				return _events.begin() + NPIPES + nspecial;
			}

			inline events_type::iterator
			begin() noexcept {
				return _events.begin() + NPIPES + nspecial;
			}

			inline events_type::iterator
			special_begin() noexcept {
				return _events.begin() + NPIPES;
			}

			inline events_type::iterator
			special_end() noexcept {
				return _events.begin() + NPIPES + nspecial;
			}

			events_type::iterator find(const poll_event& ev) {
				return std::find(this->_events.begin() + NPIPES,
					this->_events.end(), ev);
			}

			void consume_notify() {
				const size_t n = 20;
				char tmp[n];
				ssize_t c;
				while ((c = ::read(this->pipe_in(), tmp, n)) != -1);
			}

			template<class It, class Pred>
			void
			remove_ordinary_if(It first, It last, Pred pred) {
				It result = std::remove_if(first, last, pred);
				_events.erase(result.first(), last.first());
				_handlers.erase(result.second(), last.second());
			}

			template<class It, class Pred>
			inline void
			remove_specials_if(It first, It last, Pred pred) {
				_events.erase(std::remove_if(first, last, pred), last);
			}

			template<class Pred>
			inline void
			remove_fds_if(Pred pred) {
				remove_specials_if(special_begin(), special_end(), pred);
				remove_ordinary_if(
					stdx::make_paired(begin(), _handlers.begin()),
					stdx::make_paired(_events.end(), _handlers.end()),
					stdx::apply_to<0>(pred));
			}

			template<class Func>
			inline void
			for_each_ordinary_fd(Func func) {
				std::for_each(
					stdx::make_paired(begin(), _handlers.begin()),
					stdx::make_paired(_events.end(), _handlers.end()),
					[&func] (const stdx::pair<poll_event&,handler_type&>& rhs) {
						func(rhs.first, rhs.second);
					});
			}

			template<class It>
			void simulate_pollrdhup(It first, It last) {
				while (first != last) {
					if (first->probe() == 0) {
						first->setrev(poll_event::Hup);
					}
					++first;
				}
			}

			void check_dirty() {
				for_each_ordinary_fd(
					[] (poll_event& ev, handler_type& h) {
						if (h->dirty()) {
							ev.setev(poll_event::Out);
						}
					}
				);
//				size_type i = 0;
//				std::for_each(this->begin(), this->_events.end(),
//					[this,&i] (poll_event& ev) {
//						handler_type& h = this->_handlers[i];
//						if (h->dirty()) {
//							ev.setev(poll_event::Out);
//						}
//						++i;
//					}
//				);
			}
		
			template<class Callback>
			void wait(Callback callback) {

				do {
					check_dirty();
//					std::clog << "poll(): size="
//						<< this->_events.size() << std::endl;
					check_if_not<std::errc::interrupted>(::poll(this->_events.data(),
						this->_events.size(), Infinite),
						__FILE__, __LINE__, __func__);
				} while (errno == EINTR);

				this->remove_fds_if(std::mem_fn(&poll_event::bad_fd));

				#if !HAVE_DECL_POLLRDHUP
				simulate_pollrdhup(_events.begin(), _events.end());
				#endif
				handle_pipe(_events.front(), callback);
				handle_specials(special_begin(), special_end(), callback);

				for_each_ordinary_fd(
					[&callback] (poll_event& ev, handler_type& h) {
						if (h->dirty()) {
							ev.setrev(poll_event::Out);
						}
						if (!ev.bad_fd()) {
							h->operator()(ev);
						}
					}
				);

//				size_type i = 0;
//				std::for_each(this->begin(), this->_events.end(),
//					[this,&callback,&i] (poll_event& ev) {
//						handler_type& h = this->_handlers[i];
//						if (h->dirty()) {
//							ev.setrev(poll_event::Out);
//						}
//						if (!ev.bad_fd()) {
//							h->operator()(ev);
//						}
//						++i;
//					}
//				);
				this->remove_fds_if(std::mem_fn(&poll_event::bad));
			}

			template<class F>
			inline void
			handle_pipe(poll_event ev, F callback) {
				if (ev.in()) {
					this->consume_notify();
					callback(ev);
				}
			}

			template<class It, class F>
			void
			handle_specials(It first, It last, F callback) {
				while (first != last) {
					if (!first->bad_fd()) {
						callback(*first);
					}
					++first;
				}
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
			size_type nspecial = 0;

			static constexpr const size_type
			NPIPES = 1;
		};

	}

}
