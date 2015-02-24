namespace factory {

	// TODO; implement hierarchical logging of events (i.e. current level of a hierarchy is determined by
	// frame pointer in function call stack. There are several logging levels: OS, Servers, Kernels, Applications.

	namespace journaling {

		struct Logging {
			virtual int get_cpu_id() const = 0;
			virtual std::ostream& log() = 0;
		};

		__thread Logging* current_server;
	
		struct Logger {
	
			template<class T>
			Logger& operator<<(const T& object) {
				if (current_server != nullptr) {
					current_server->log() << object;
				}
				return *this;
			}
	
			Logger& operator<<(std::ostream& ( *pf )(std::ostream&)) {
				if (current_server != nullptr) {
					current_server->log() << pf;
				}
				return *this;
			}
	
			static const char SEPARATOR;
		};
		const char Logger::SEPARATOR = ',';
	
		struct Log_event {
	
			explicit Log_event(const std::string& name): tag(name) {}
	
			friend Logger& operator<<(Logger& out, const Log_event& rhs) {
				if (current_server != nullptr) {
					current_server->log()
						<< rhs.tag << Logger::SEPARATOR
						<< current_server->get_cpu_id() << Logger::SEPARATOR
						<< now() << Logger::SEPARATOR;
				}
				return out;
			}
	
		private:
			std::string tag;
		};
	
		void log_event(const char* name, Time duration) {
			if (current_server != nullptr) {
				current_server->log() << name
					<< ',' << current_server->get_cpu_id()
					<< ',' << now()
					<< ',' << duration
					<< '\n';
			}
		}
	

		template<class Q>
		class Journaling_pool: public Q {
		private:
			typedef typename Q::value_type T;

		public:
			Journaling_pool(): Q() {}
			virtual ~Journaling_pool() {}

			void push(const T& val) {
				Time t0 = now();
				Q::push(val);
				Time t1 = now();
				log_event("queue", t1-t0);
				Logger logger;
				logger << Log_event("push") << std::endl;
			}

			T& front() {
				Time t0 = now();
				T& val = Q::front();
				Time t1 = now();
				log_event("queue", t1-t0);
				return val;
			}

			void pop() {
				Time t0 = now();
				Q::pop();
				Time t1 = now();
				log_event("queue", t1-t0);
			}

		};

		template<class P>
		class Journaling_production_strategy: public P {
		public:
			template<class S>
			explicit Journaling_production_strategy(const S& upstream):
				P(upstream), id(counter++) {}

			typedef typename P::Profiler Profiler_base;
//			typedef typename P::Iprofiler Iprofiler_base;
			typedef typename P::Rprofiler Rprofiler_base;
			typedef typename P::Advisor Advisor_base;
			template<class A> using Carrier_base = typename P::template Carrier<A>;

			template<class I, class A>
			int operator()(A* a, I& upstream) {
				Time t0 = now();
				int ret = P::operator()(a, upstream);
				Time t1 = now();
				log_event("strategy_up", t1-t0);
				log_state(a, upstream, ret);
				return ret;
			}

			template<class I, class A>
			void log_state(A* a, I& upstream, int p) {
				int n = upstream.size();
				std::stringstream tmp;
				tmp << "cache";
				tmp << ',' << id;
				tmp << ',' << now();
				tmp << ',' << a->name();
				tmp << *this;
				tmp << ',' << p;
				for (int i=0; i<n; ++i) {
					tmp << ',' << -upstream[i].dynamic_metric();
				}
				tmp << '\n';
				if (current_server != nullptr) {
					current_server->log() << tmp.str();
				}
			}

			struct Rprofiler: public Rprofiler_base {
				template<class A>
				void operator()(Carrier_base<A>& carrier) {
					std::string actor_name = carrier.get_kernel()->name();
					Time t0 = now();
					Rprofiler_base::operator()(carrier);
					Time t1 = now();

					Time profile_time = t1 - t0 - carrier.action_time();

					Logger log;
					log << Log_event("profile") 
						<< profile_time << Logger::SEPARATOR
						<< actor_name << Logger::SEPARATOR
						<< static_cast<const Rprofiler_base&>(*this)
						<< std::endl;
				}
			};

			template<class A>
			struct Carrier: public Carrier_base<A> {

				Carrier(): Carrier_base<A>() {}
				explicit Carrier(A* a): Carrier_base<A>(a) {}

				void act() { 
					std::string name = Carrier_base<A>::get_kernel()->name();
					std::string type_name = typeid(*Carrier_base<A>::get_kernel()).name();
					Time t0 = now();
					Carrier_base<A>::act();
					Time t1 = now();
					Time t_act = t1-t0;
					Logger logger;
					logger << Log_event("act") << t_act << Logger::SEPARATOR << name << std::endl;
					logger << Log_event("debug_act") << t_act << Logger::SEPARATOR << type_name << std::endl;
				}
			};

			struct Advisor: public Advisor_base {
				template<class A, class S>
				bool is_ready_to_go(const Carrier_base<A>& carrier, const S* server, Profiler_base& profiler) const {
					bool is_ready = Advisor_base::is_ready_to_go(carrier, server, profiler);
					if (is_ready) {
						const A* kernel = carrier.get_kernel();
						Logger logger;
						logger << Log_event("resubmit") << kernel->id() << Logger::SEPARATOR << 1 << std::endl;
					}
					return is_ready;
				}
			};
		private:
			int id;
			static int counter;
		};

		template<class P>
		int Journaling_production_strategy<P>::counter = 0;

		template<class P>
		class Journaling_reduction_strategy: public P {
		public:
			template<class S>
			explicit Journaling_reduction_strategy(const S& upstream): P(upstream) {}

			template<class A> using Carrier_base = typename P::template Carrier<A>;

			template<class I, class A>
			int operator()(const factory::components::Kernel_pair<A>* pair, I& upstream) {
				Time t0 = now();
				int ret = P::operator()(pair, upstream);
				Time t1 = now();
				log_event("strategy_down", t1-t0);
				return ret;
			}

			template<class A>
			struct Carrier: public Carrier_base<A> {

				Carrier(): Carrier_base<A>() {}
				explicit Carrier(A* a): Carrier_base<A>(a) {}

				void act() { 
					A* subordinate = Carrier_base<A>::get_kernel()->subordinate();
					A* principal = Carrier_base<A>::get_kernel()->principal();
					std::string type_name1 = typeid(*subordinate).name();
					std::string type_name2 = typeid(*principal).name();
					Time t0 = now();
					Carrier_base<A>::act();
					Time t1 = now();
		
					Logger logger;
					logger << Log_event("react")
						<< t1-t0 << Logger::SEPARATOR
						<< subordinate->name() << Logger::SEPARATOR
						<< principal->name() << std::endl;
					logger << Log_event("debug_react")
						<< t1-t0 << Logger::SEPARATOR
						<< type_name1 << Logger::SEPARATOR
						<< type_name2 << std::endl;
				}
			};
		};

		template<class A>
		struct Id {
			Id(const A* a): i(a == nullptr ? 0 : a->id()) {}
			friend std::ostream& operator<<(std::ostream& out, const Id& rhs) {
				return out << 'A' << std::setw(10) << std::setfill('0') << rhs.i;
			}
		private:
			int i;
		};

		struct Graph_sequence {
			static int next_number_in_sequence() {
				static std::atomic<int> kernels(0);
				return kernels++;
			}
		};

		template<class P>
		class Graphing_reduction_strategy: public P {
		public:
			template<class S>
			explicit Graphing_reduction_strategy(const S& upstream): P(upstream) {}
			template<class A> using Carrier_base = typename P::template Carrier<A>;

			template<class A>
			struct Carrier: public Carrier_base<A> {

				Carrier(): Carrier_base<A>() {}
				explicit Carrier(A* a): Carrier_base<A>(a) {}

				void act() {
					A* subordinate = Carrier_base<A>::get_kernel()->subordinate();
					A* principal = Carrier_base<A>::get_kernel()->principal();
					if (current_server != nullptr) {
						current_server->log() << "graph,"
							<< Graph_sequence::next_number_in_sequence() << ',' << Id<A>(subordinate)
							<< " -> " << Id<A>(principal) << std::endl;
						if (subordinate->parent() == principal) {
							current_server->log() << "graph,"
								<< Graph_sequence::next_number_in_sequence()
								<< ",delete," << Id<A>(subordinate) << std::endl;
						}
					}
					Carrier_base<A>::act();
				}

			};
		};

		template<class P>
		class Graphing_production_strategy: public P {
		public:
			template<class S>
			explicit Graphing_production_strategy(const S& upstream): P(upstream) {}
			template<class A> using Carrier_base = typename P::template Carrier<A>;

			template<class A>
			struct Carrier: public Carrier_base<A> {

				Carrier(): Carrier_base<A>() {}
				explicit Carrier(A* a): Carrier_base<A>(a) {}

				void act() { 
					int cnt = Graph_sequence::next_number_in_sequence();
					if (current_server != nullptr) {
						Id<A> this_id(Carrier_base<A>::get_kernel());
						current_server->log() << "graph," << cnt
							   << ',' << Id<A>(Carrier_base<A>::get_kernel()->parent()) << " -> " << this_id
							   << ';' << this_id << " [label=\""
							   << Carrier_base<A>::get_kernel()->name() << "\"]" << std::endl;
					}
					Carrier_base<A>::act();
				}

			};
		};

		template<class K>
		class Journaling_kernel: public K {
		public:
			Journaling_kernel(): _id(next_id()) {}
			int id() const { return _id; }
			int next_id() const {
				static std::atomic<int> ids(0);
				return ids++;
			}
			virtual const char* name() const { return ""; }
		private:
			int _id;
		};


		template<class S>
		struct Journaling_Rserver: public S, public Logging {
			Journaling_Rserver(int cpu):
				S(cpu),
				time(0),
				_cpu(cpu),
				_log()
			{}
			~Journaling_Rserver() { _log.close(); }

			int get_cpu_id() const { return _cpu; }
			std::ostream& log() { return _log; }

			void open_log_file() {
				std::stringstream filename;
				filename << typeid(S).name() << '.' << std::setfill('0') << std::setw(2) << _cpu << ".log";
				_log.open(filename.str());
			}

			void process_kernel(typename S::template Carrier<typename S::Kernel>& carrier) {
				Time t0 = now();
				S::process_kernel(carrier);
				Time t1 = now();
				time += t1-t0;
			}

			void wait_for_an_kernel() {
				Logger logger;
				logger << Log_event("cycle") << 1 << std::endl;
				S::wait_for_an_kernel();
			}

			void serve() {
				open_log_file();
				current_server = this;

				Logger logger;
				logger << Log_event("launch") << current_cpu() << std::endl;

				Time t0 = now();
				S::serve();
				Time t1 = now();

				Time all_time = t1-t0;
				Time stale_time = all_time - time;

				logger << Log_event("stale") << stale_time << std::endl;
				logger << Log_event("all") << all_time << std::endl;
				logger << Log_event("termination") << current_cpu() << std::endl;
			}

		private:
			Time time;
			int _cpu;
			std::ofstream _log;
		};

	}

}
