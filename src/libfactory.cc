#include <factory/factory.hh>
#if HAVE_GCC_ABI_DEMANGLE
	#include <cxxabi.h>
#endif
#if HAVE_EXECINFO_H
	#include <execinfo.h>
#endif

namespace factory {
	namespace components {
		/// ``Do not trust file descriptors 0, 1, 2''
		/// http://www.lst.de/~okir/blackhats/node41.html
		struct Auto_open_standard_file_descriptors {
			Auto_open_standard_file_descriptors() {
				const int MAX_ITER = 3;
				int fd = 0;
				for (int i=0; i<MAX_ITER && fd<=2; ++i) {
					fd = check(::open("/dev/null", O_RDWR),
						__FILE__, __LINE__, __func__);
					if (fd > 2) {
						check(::close(fd),
							__FILE__, __LINE__, __func__);
					}
				}
			}
		} __auto_open_standard_file_descriptors;
		struct Auto_filter_bad_chars_on_cout_and_cerr {
			Auto_filter_bad_chars_on_cout_and_cerr() {
				this->filter(std::cout);
				this->filter(std::cerr);
			}
		private:
			void filter(std::ostream& str) {
				std::streambuf* orig = str.rdbuf();
				str.rdbuf(new Filter_buf<std::ostream::char_type>(orig));
			}
		} __auto_filter_bad_chars_on_cout_and_cerr;
	
		struct Auto_check_endiannes {
			Auto_check_endiannes() {
				union Endian {
					constexpr Endian() {}
					uint32_t i = UINT32_C(1);
					uint8_t b[4];
				} endian;
				if ((is_network_byte_order() && endian.b[0] != 0)
					|| (!is_network_byte_order() && endian.b[0] != 1))
				{
					throw Error("endiannes was not correctly determined at compile time",
						__FILE__, __LINE__, __func__);
				}
			}
		} __factory_auto_check_endiannes;

		struct Auto_set_terminate_handler {
			Auto_set_terminate_handler() {
				Auto_set_terminate_handler::_old_handler
					= std::set_terminate(Auto_set_terminate_handler::error_printing_handler);
			}
		private:
			static void error_printing_handler() noexcept {
				static volatile bool called = false;
				if (called) { return; }
				called = true;
				bool print_backtrace = true;
				std::exception_ptr ptr = std::current_exception();
				if (ptr) {
					try {
						std::rethrow_exception(ptr);
					} catch (Error& err) {
						Logger<Level::ERR>() << Error_message(err, __FILE__, __LINE__, __func__) << std::endl;
						print_backtrace = false;
					} catch (std::exception& err) {
						Logger<Level::ERR>() << String_message(err, __FILE__, __LINE__, __func__) << std::endl;
					} catch (...) {
						Logger<Level::ERR>() << String_message(UNKNOWN_ERROR, __FILE__, __LINE__, __func__) << std::endl;
					}
				} else {
					Logger<Level::ERR>() << String_message("terminate called without an active exception",
						__FILE__, __LINE__, __func__) << std::endl;
				}
				if (print_backtrace) {
					print_stack_trace();
				}
				stop_all_factories(true);
				if (Auto_set_terminate_handler::_old_handler) {
					Auto_set_terminate_handler::_old_handler();
				}
				std::abort();
			}

#if defined(HAVE_BACKTRACE)
			static void print_stack_trace() noexcept {
				static const backtrace_size_t MAX_ENTRIES = 64;
				void* stack[MAX_ENTRIES];
				backtrace_size_t num_entries = ::backtrace(stack, MAX_ENTRIES);
				::backtrace_symbols_fd(stack, num_entries, STDERR_FILENO);
			}
#else
			static void print_stack_trace() {}
#endif
		private:
			static std::terminate_handler _old_handler;
		} __factory_auto_set_terminate_handler;

		std::terminate_handler Auto_set_terminate_handler::_old_handler = nullptr;

	}
	namespace components {
	Backtrace::Backtrace() {
	#if HAVE_BACKTRACE
		void* stack[MAX_ENTRIES];
		this->num_entries = ::backtrace(stack, MAX_ENTRIES);
		this->symbols = ::backtrace_symbols(stack, this->num_entries);
	#endif
	}
	std::ostream& operator<<(std::ostream& out, const Backtrace& rhs) {
	#if HAVE_BACKTRACE
		if (rhs.num_entries <= 1) {
			out << "<no entries>\n";
		} else {
			out << '[';
			std::vector<std::string> callstack(rhs.num_entries-1);
			std::transform(rhs.symbols+1, rhs.symbols + rhs.num_entries, callstack.begin(),
				[&out](const char* line) {
					std::string tmp = line;
					std::size_t beg = tmp.find('(');
					std::size_t end = tmp.find('+', beg == std::string::npos ? 0 : beg+1);
					if (beg == std::string::npos && end != std::string::npos) {
						// try BSD-like backtrace
						if (end > 1 && tmp[end-1] == ' ') {
							beg = tmp.rfind(' ', end-2);
							--end;
						}
					}
					if (beg != std::string::npos
						&& end != std::string::npos
						&& beg < end)
					{
						tmp[end] = '\0';
					#if HAVE_GCC_ABI_DEMANGLE
						int status = 0;
						char* ret = abi::__cxa_demangle(&tmp[beg+1], 0, 0, &status);
					#else
						int status = -1;
						char* ret = nullptr;
					#endif
						if (status == 0) {
							tmp = ret;
//							tmp[beg] = '\0';
//							out << ret << ',';
//							out << &tmp[0]
//								<< '(' << ret
//								<< line + end << '\n';
							::free(ret);
						} else {
							return tmp.substr(beg+1, end-beg-1);
//							tmp = &tmp[beg+1];
						}
					} else {
//						out << line << ',';
					}
					return tmp;
				}
			);
			components::intersperse_iterator<std::string> it(out, ",");
			std::copy(callstack.begin(), callstack.end(), it);
			out << ']';
		}
	#else
		out << "<backtrace is disabled>\n";
	#endif
		return out;
	}
	}
}

namespace factory {
	namespace components {
		struct All_factories {
			typedef Resident F;
			All_factories() {
				init_signal_handlers();
			}
			void add(F* f) {
				this->_factories.push_back(f);
				Logger<Level::SERVER>() << "add factory: size=" << _factories.size() << std::endl;
			}
			void stop_all(bool now=false) {
				Logger<Level::SERVER>() << "stop all: size=" << _factories.size() << std::endl;
				std::for_each(_factories.begin(), _factories.end(),
					[now] (F* rhs) { now ? rhs->stop_now() : rhs->stop(); });
			}
			void print_all_endpoints(std::ostream& out) const {
				std::vector<Endpoint> addrs;
				std::for_each(this->_factories.begin(), this->_factories.end(),
					[&addrs] (const F* rhs) {
						Endpoint a = rhs->addr();
						if (a) { addrs.push_back(a); }
					}
				);
				if (addrs.empty()) {
					out << Endpoint();
				} else {
					intersperse_iterator<Endpoint> it(out, ",");
					std::copy(addrs.begin(), addrs.end(), it);
				}
			}

		private:
			void init_signal_handlers() {
				Action shutdown(emergency_shutdown);
				this_process::bind_signal(SIGTERM, shutdown);
				this_process::bind_signal(SIGINT, shutdown);
				this_process::bind_signal(SIGPIPE, Action(SIG_IGN));
#ifndef FACTORY_NO_BACKTRACE
				this_process::bind_signal(SIGSEGV, Action(print_backtrace));
#endif
			}

			static void emergency_shutdown(int sig) noexcept {
				stop_all_factories();
				static int num_calls = 0;
				static const int MAX_CALLS = 3;
				++num_calls;
				std::clog << "Ctrl-C shutdown." << std::endl;
				if (num_calls >= MAX_CALLS) {
					std::clog << "MAX_CALLS reached. Aborting." << std::endl;
					std::abort();
				}
			}

			[[noreturn]]
			static void print_backtrace(int) {
				throw Error("segmentation fault", __FILE__, __LINE__, __func__);
			}

			std::vector<F*> _factories;
		} __all_factories;

		void stop_all_factories(bool now) {
			__all_factories.stop_all(now);
		}

		void print_all_endpoints(std::ostream& out) {
			__all_factories.print_all_endpoints(out);
		}

		void register_factory(Resident* factory) {
			__all_factories.add(factory);
		}
	}
}

#ifdef HAVE___UINT128_T
namespace std {
	std::ostream& operator<<(std::ostream& o, __uint128_t rhs) {
		static const unsigned int NBITS = sizeof(__uint128_t)
			* std::numeric_limits<unsigned char>::digits;
		std::ostream::sentry s(o);
		if (!s) { return o; }
		int radix = uint128_get_radix(o);
		if (rhs == 0) { o << '0'; }
		else {
			// at worst it will be NBITS digits (base 2) so make our buffer
			// that plus room for null terminator
    		char buf[NBITS + 1] = {};

    		__uint128_t ii = rhs;
    		int i = NBITS - 1;

    		while (ii != 0 && i) {
				__uint128_t res = ii/radix;
				__uint128_t remainder = ii%radix;
				ii = res;
        		buf[--i] = "0123456789abcdefghijklmnopqrstuvwxyz"[remainder];
    		}

			o << buf + i;
		}
		return o;
	}

	std::istream& operator>>(std::istream& in, __uint128_t& rhs) {
		std::istream::sentry s(in);
		if (s) {
			rhs = 0;
			unsigned int radix = uint128_get_radix(in);
			char ch;
			while (in >> ch) {
				unsigned int n = radix;
				switch (radix) {
					case 16: {
						ch = std::tolower(ch);
						if (ch >= 'a' && ch <= 'f') {
							n = ch - 'a' + 10;
						} else if (ch >= '0' && ch <= '9') {
							n = ch - '0';
						}
					} break;
					case 8:	
						if (ch >= '0' && ch <= '7') {
							n = ch - '0';
						}
						break;
					case 10:
					default:
						if (ch >= '0' && ch <= '9') {
							n = ch - '0';
						}
						break;
				}
				if (n == radix) {
					break;
				}

				rhs *= radix;
				rhs += n;
			}
		}
		return in;
	}
}
#endif
namespace factory {
	namespace components {
		Underflow underflow;
		End_packet end_packet;
	}
}

namespace factory {
	namespace components {
#if defined(FACTORY_NO_CPU_BINDING)
		void thread_affinity(size_t) {}
#else
#if defined(HAVE_CPU_SET_T)
	#if defined(HAVE_SCHED_H)
	#include <sched.h>
	#elif defined(HAVE_SYS_CPUSET_H)
	#include <sys/cpuset.h>
	#endif
	struct CPU {
		typedef ::cpu_set_t Set;

		CPU(size_t cpu) {
			size_t num_cpus = total_cpus();
			cpuset = CPU_ALLOC(num_cpus);
			_size = CPU_ALLOC_SIZE(num_cpus);
			CPU_ZERO_S(size(), cpuset);
			CPU_SET_S(cpu%num_cpus, size(), cpuset);
		}

		~CPU() { CPU_FREE(cpuset); }

		size_t size() const { return _size; }
		Set* set() { return cpuset; }

	private:
		Set* cpuset;
		size_t _size;
	};
#endif
#if HAVE_DECL_PTHREAD_SETAFFINITY_NP
#include <pthread.h>
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check(::pthread_setaffinity_np(::pthread_self(),
				cpu.size(), cpu.set()),
				__FILE__, __LINE__, __func__);
		}
#elif HAVE_DECL_SCHED_SETAFFINITY
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check(::sched_setaffinity(0, cpu.size(), cpu.set()),
				__FILE__, __LINE__, __func__);
		}
#elif HAVE_SYS_PROCESSOR_H
#include <sys/processor.h>
		void thread_affinity(size_t) {
			::processor_bind(P_LWPID, P_MYID, cpu_id%total_cpus(), 0);
		}
#elif HAVE_SYS_CPUSET_H
		void thread_affinity(size_t c) {
			CPU cpu(c);
			check(::cpuset_setaffinity(CPU_LEVEL_WHICH,
				CPU_WHICH_TID, -1, cpu.size(), cpu.set(),
				__FILE__, __LINE__, __func__));
		}
#else
// no cpu binding
		void thread_affinity(size_t) {}
#endif
#endif
	}

}

namespace factory {
	Factory __factory;

	namespace components {
		Id factory_start_id() {
			constexpr static const Id DEFAULT_START_ID = 1000;
			Id i = this_process::getenv("START_ID", DEFAULT_START_ID);
			if (i == ROOT_ID) {
				i = DEFAULT_START_ID;
				Logger<Level::COMPONENT>() << "Bad START_ID value: " << ROOT_ID << std::endl;
			}
			Logger<Level::COMPONENT>() << "START_ID = " << i << std::endl;
			return i;
		}

		Id factory_generate_id() {
			static std::atomic<Id> counter(factory_start_id());
			return counter++;
		}
		Spin_mutex __logger_mutex;
		std::vector<Address_range> discover_neighbours() {
		
			struct ::ifaddrs* ifaddr;
			check(::getifaddrs(&ifaddr),
				__FILE__, __LINE__, __func__);
		
			std::set<Address_range> ranges;
		
			for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	
				if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
					// ignore non-internet networks
					continue;
				}
	
				Endpoint addr(*ifa->ifa_addr);
				if (addr.address() == Endpoint("127.0.0.1", 0).address()) {
					// ignore localhost and non-IPv4 addresses
					continue;
				}
	
				Endpoint netmask(*ifa->ifa_netmask);
				if (netmask.address() == Endpoint("255.255.255.255",0).address()) {
					// ignore wide-area networks
					continue;
				}
		
				uint32_t addr_long = addr.address();
				uint32_t mask_long = netmask.address();
		
				uint32_t start = (addr_long & mask_long) + 1;
				uint32_t end = (addr_long & mask_long) + (~mask_long);
		
				ranges.insert(Address_range(start, addr_long));
				ranges.insert(Address_range(addr_long+1, end));
			}
		
			// combine overlaping ranges
			std::vector<Address_range> sorted_ranges;
			Address_range prev_range;
			std::for_each(ranges.cbegin(), ranges.cend(),
				[&sorted_ranges, &prev_range](const Address_range& range)
			{
				if (prev_range.empty()) {
					prev_range = range;
				} else {
					if (prev_range.overlaps(range)) {
						prev_range += range;
					} else {
						sorted_ranges.push_back(prev_range);
						prev_range = range;
					}
				}
			});
		
			if (!prev_range.empty()) {
				sorted_ranges.push_back(prev_range);
			}
		
			std::for_each(sorted_ranges.cbegin(), sorted_ranges.cend(),
				[] (const Address_range& range)
			{
				std::clog << Address(range.start()) << '-' << Address(range.end()) << '\n';
			});
		
			::freeifaddrs(ifaddr);
		
			return sorted_ranges;
		}
	}
}

namespace factory {
	namespace components {
		std::mutex __kernel_delete_mutex;
		std::atomic<int> __num_rservers(0);
		std::condition_variable __kernel_semaphore;
		void register_server() { ++__num_rservers; }
		void global_barrier(std::unique_lock<std::mutex>& lock) {
			if (--__num_rservers <= 0) {
				__kernel_semaphore.notify_all();
			}
			__kernel_semaphore.wait(lock, [] () {
				return __num_rservers <= 0;
			});
		}
		std::random_device rng;
	}
}
