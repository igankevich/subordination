#ifndef SYSX_EVENT_HH
#define SYSX_EVENT_HH

#include <poll.h>
#include <sys/socket.h>
#if !defined(POLLRDHUP)
	#define FACTORY_POLLRDHUP 0
#else
	#define FACTORY_POLLRDHUP POLLRDHUP
#endif

#include <cassert>

#include <stdx/paired_iterator.hh>
#include <stdx/unlock_guard.hh>

#include <sysx/bits/check.hh>
#include <sysx/pipe.hh>

namespace sysx {

	typedef struct ::pollfd basic_event;

	struct poll_event: public basic_event {

		typedef decltype(basic_event::events) legacy_event;

		enum event_type: legacy_event {
			In = POLLIN,
			Out = POLLOUT,
			Hup = POLLHUP | FACTORY_POLLRDHUP,
			Err = POLLERR | POLLNVAL,
			Inout = POLLIN | POLLOUT,
			Def = FACTORY_POLLRDHUP
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
			basic_event{f,ev|Def,rev} {}

		constexpr legacy_event
		revents() const noexcept {
			return this->basic_event::revents;
		}

		constexpr legacy_event
		events() const noexcept {
			return this->basic_event::events;
		}

		inline void
		disable() noexcept {
			this->basic_event::fd = -1;
		}

		constexpr fd_type
		fd() const noexcept {
			return this->basic_event::fd;
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
			this->basic_event::events |= rhs;
		}

		inline void
		unsetev(event_type rhs) noexcept {
			this->basic_event::events &= ~rhs;
		}

		inline void
		setrev(event_type rhs) noexcept {
			this->basic_event::revents |= rhs;
		}

		inline void
		unsetrev(event_type rhs) noexcept {
			this->basic_event::revents &= ~rhs;
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

		explicit constexpr
		operator bool() const noexcept {
			return !this->bad() && !this->bad_fd();
		}

		constexpr bool
		operator !() const noexcept {
			return !operator bool();
		}

		inline
		poll_event& operator=(const poll_event&) noexcept = default;

		friend std::ostream&
		operator<<(std::ostream& out, const poll_event& rhs) {
			return out << "{fd=" << rhs.fd() << ",rev="
				<< (rhs.in() ? 'r' : '-')
				<< (rhs.out() ? 'w' : '-')
				<< (rhs.hup() ? 'c' : '-')
				<< (rhs.err() ? 'e' : '-')
				<< ",ev="
				<< (rhs.events() & poll_event::In ? 'r' : '-')
				<< (rhs.events() & poll_event::Out ? 'w' : '-')
				<< '}';
		}

	};

	static_assert(sizeof(poll_event) == sizeof(basic_event),
		"The size of poll_event does not match the size of ``struct pollfd''.");

	template<class Handler>
	struct event_poller {

		typedef Handler handler_type;
		typedef std::vector<poll_event> events_type;
		typedef std::vector<handler_type> handlers_type;
		typedef events_type::size_type size_type;

		inline
		event_poller():
		_pipe(),
		_events{poll_event{this->_pipe.in().get_fd(), poll_event::In}}
		{
			assert_invariant();
		}

		~event_poller() = default;
		event_poller(const event_poller&) = delete;
		event_poller& operator=(const event_poller&) = delete;

		inline
		event_poller(event_poller&& rhs) noexcept:
		_pipe(std::move(rhs._pipe)),
		_events(std::move(rhs._events)),
		_handlers(std::move(rhs._handlers)),
		_specials(std::move(rhs._specials))
		{
			assert_invariant();
		}

		void
		notify_one() noexcept {
			char c = '!';
			this->_pipe.out().write(&c, sizeof(char));
		}

		void
		notify_all() noexcept {
			notify_one();
		}

		template<class Lock, class Pred>
		void
		wait(Lock& lock, Pred pred) {
			insert_pending_specials();
			bool success = false;
			while (!success && !pred()) {
				stdx::unlock_guard<Lock> g(lock);
				success = do_wait();
			}
		}

		template<class Lock>
		void
		wait(Lock& lock) {
			wait(lock, [] () { return false; });
		}

		inline fd_type
		pipe_in() const noexcept {
			return this->_pipe.in().get_fd();
		}

		inline void
		clear() noexcept {
			this->_events.erase(special_begin(), _events.end());
			this->_nspecials = 0;
			this->_handlers.clear();
			assert_invariant();
		}

		void
		insert_special(poll_event ev) {
			_specials.push_back(ev);
		}

		void
		emplace(poll_event ev, handler_type&& handler) {
			this->_events.push_back(ev);
			this->_handlers.emplace_back(std::move(handler));
			assert_invariant();
		}

		void
		disable(fd_type fd) noexcept {
			events_type::iterator pos = this->find(poll_event{fd});
			if (pos != this->_events.end()) {
				pos->disable();
			}
		}

		template<class Func>
		inline void
		for_each_ordinary_fd(Func func) {
			assert_invariant();
			std::for_each(
				stdx::make_paired(ordinary_begin(), _handlers.begin()),
				stdx::make_paired(_events.end(), _handlers.end()),
				[&func] (const stdx::pair<poll_event&,handler_type&>& rhs) {
					func(rhs.first, rhs.second);
				});
		}

		template<class Func>
		inline void
		for_each_special_fd(Func func) {
			std::for_each(special_begin(), special_end(), func);
		}

		template<class Func>
		inline void
		for_each_pipe_fd(Func func) {
			std::for_each(pipes_begin(), pipes_end(), func);
		}

	private:

		inline events_type::iterator
		pipes_begin() noexcept {
			return _events.begin();
		}

		inline events_type::const_iterator
		pipes_begin() const noexcept {
			return _events.begin();
		}

		inline events_type::iterator
		pipes_end() noexcept {
			return _events.begin() + NPIPES;
		}

		inline events_type::const_iterator
		pipes_end() const noexcept {
			return _events.begin() + NPIPES;
		}

		inline events_type::const_iterator
		ordinary_begin() const noexcept {
			return _events.begin() + NPIPES + _nspecials;
		}

		inline events_type::iterator
		ordinary_begin() noexcept {
			return _events.begin() + NPIPES + _nspecials;
		}

		inline events_type::const_iterator
		ordinary_end() const noexcept {
			return _events.end();
		}

		inline events_type::iterator
		ordinary_end() noexcept {
			return _events.end();
		}

		inline events_type::iterator
		special_begin() noexcept {
			return _events.begin() + NPIPES;
		}

		inline events_type::const_iterator
		special_begin() const noexcept {
			return _events.begin() + NPIPES;
		}

		inline events_type::iterator
		special_end() noexcept {
			return _events.begin() + NPIPES + _nspecials;
		}

		inline events_type::const_iterator
		special_end() const noexcept {
			return _events.begin() + NPIPES + _nspecials;
		}

		inline events_type::iterator
		find(const poll_event& ev) noexcept {
			return std::find(this->_events.begin() + NPIPES,
				this->_events.end(), ev);
		}

		template<class It, class Pred>
		void
		remove_ordinary_if(It first, It last, Pred pred) {
			It result = std::remove_if(first, last, pred);
			_events.erase(result.first(), last.first());
			_handlers.erase(result.second(), last.second());
			assert_invariant();
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
				stdx::make_paired(ordinary_begin(), _handlers.begin()),
				stdx::make_paired(_events.end(), _handlers.end()),
				stdx::apply_to<0>(pred));
			assert_invariant();
		}

		template<class It>
		void
		check_hup(It first, It last) {
			while (first != last) {
				if (first->probe() == 0) {
					first->setrev(poll_event::Hup);
				}
				++first;
			}
		}

		void
		insert_pending_specials() {
			_events.insert(this->ordinary_begin(), _specials.begin(), _specials.end());
			_nspecials += _specials.size();
			_specials.clear();
			assert_invariant();
		}

		bool
		do_wait() {

			bool success = false;

			remove_fds_if(std::logical_not<poll_event>());
			insert_pending_specials();

//			std::clog << "before poll(this=" << *this << ")" << std::endl;
			bits::check_if_not<std::errc::interrupted>(
				::poll(this->_events.data(),
				this->_events.size(), no_timeout),
				__FILE__, __LINE__, __func__);
			success = errno != EINTR;
//			std::clog << "after poll(this=" << *this << ")" << std::endl;

			if (success) {
				#if !defined(POLLRDHUP)
				check_hup(_events.begin(), _events.end());
				#endif
				for_each_pipe_fd(&event_poller::consume);
			}

			return success;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const event_poller& rhs) {
			std::ostream::sentry s(out);
			if (s) {
				out << '{';
				std::copy(rhs._events.begin(), rhs._events.end(),
					stdx::intersperse_iterator<poll_event>(out, ","));
				out << '}';
			}
			return out;
		}

		static void
		consume(const poll_event& ev) noexcept {
			if (ev.in()) {
				const size_t n = 20;
				char tmp[n];
				ssize_t c;
				while ((c = ::read(ev.fd(), tmp, n)) != -1);
			}
		}

		inline void
		assert_invariant() const {
			assert(pipes_begin() == _events.begin());
			assert(pipes_end() == special_begin());
			assert(special_end() == ordinary_begin());
			assert(ordinary_end() == _events.end());
			assert(pipes_begin() <= pipes_end());
			assert(special_begin() <= special_end());
			// TODO 2016-02-26 sometimes it fails on the following line
			assert(ordinary_begin() <= ordinary_end());
			assert(special_end() - special_begin() == static_cast<std::make_signed<size_type>::type>(_nspecials));
			assert(pipes_end() - pipes_begin() == NPIPES);
			assert(ordinary_end() - ordinary_begin() == _handlers.end() - _handlers.begin());
		}

		sysx::pipe _pipe;
		events_type _events;
		handlers_type _handlers;
		std::vector<poll_event> _specials;
		size_type _nspecials = 0;

		static constexpr const size_type
		NPIPES = 1;

		enum: int { no_timeout = -1 };
	};

}

#endif // SYSX_EVENT_HH
