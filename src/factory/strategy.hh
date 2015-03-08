namespace factory {

	/// Base 2 logarithm. Works for positive x only.
	int log2(std::int64_t x) {
		if (x < 0) {
			std::stringstream tmp;
			tmp << "log2(x) works for positive x only. X = " << x;
			throw std::runtime_error(tmp.str());
		}
		int n = 0;
		while (x >>= 1) n++;
		return n;
	}
	
	/// Two to the power of n. Works for positive n only.
	std::int64_t pow2(int n) { return INTMAX_C(1) << n; }

	namespace components {

		template<class Kernel>
		struct Round_robin {

			template<class Server>
			class Strategy: public Server {
			public:
//				typedef typename Server::Kernel Kernel;

				Strategy(): _cursor(0) {}

//				using Server::send;

				void send(Kernel* kernel) {
					Logger(Level::STRATEGY) << "Round_robin::send()" << std::endl;
					if (kernel->moves_upstream()) {
						int n = Server::_upstream.size();
						if (n > 0) {
							int i = _cursor = (_cursor + 1) % n;
							Server::_upstream[i]->send(kernel);
						} else {
							std::cerr << "FATAL. Deleting kernel because there are no upstream servers." << std::endl;
							delete kernel;
						}
					} else {
						size_t n = Server::_upstream.size();
						if (n > 0) {
							size_t i = std::size_t(kernel->principal()) / ALIGNMENT * PRIME % n;
							Server::_upstream[i]->send(kernel);
						} else {
							std::cerr << "FATAL. Deleting kernel because there are no upstream servers." << std::endl;
							delete kernel;
						}
					}
				}

			private:
				std::atomic<int> _cursor;

				static const size_t ALIGNMENT = 64;
				static const size_t PRIME     = 7;
			};

			template<class Server>
			struct Iprofiler: public Server {};

			template<class Server>
			struct Rprofiler: public Server {
				void process_kernel(Kernel* kernel) {
					Logger(Level::STRATEGY) << "Round_robin::process_kernel()" << std::endl;
                    kernel->run_act();
				}
			};
		};
		
//		class Stochastic_round_robin {
//			typedef int Int;
//		public:
//			template<class S>
//			explicit Stochastic_round_robin(const S& upstream):
//				cursor(0),
//				cache(new Int[upstream.size()]),
//				is_resetting(new bool[upstream.size()]),
//				mtx(),
//				size(upstream.size())
//			{}
//		
//			~Stochastic_round_robin() { delete[] cache; delete[] is_resetting; }
//		
//			template<class A, class I>
//			int operator()(A*, I& upstream) {
//		
//				int n = upstream.size();
//				num_samples = upstream.num_samples();
//				
//				std::lock_guard<std::mutex> lock(mtx);
//		
//				// Cache efficiency of upstream servers and find the minimum value.
//				for (int i=0; i<n; ++i) {
//					cache[i] = -upstream[i].dynamic_metric();
//					is_resetting[i] = upstream[i].is_resetting();
//				}
//			
//				bool at_least_one_is_resetting = false;
//				for (int i=0; i<n; ++i) {
//					if (is_resetting[i]) {
//						at_least_one_is_resetting = true;
//						break;
//					}
//				}
//				if (at_least_one_is_resetting) {
//					for (int i=0; i<n; ++i) {
//						cache[i] = upstream[i].static_metric();
//					}
//		//			Int efficiency = 0;
//		//			int m = 0;
//		//			for (int i=0; i<n; ++i) {
//		//				if (!is_resetting[i]) {
//		//					efficiency += cache[i];
//		//					m++;
//		//				}
//		//			}
//		//			if (m > 0) {
//		//				efficiency /= m;
//		//			}
//		//			for (int i=0; i<n; ++i) {
//		//				if (is_resetting[i]) {
//		//					cache[i] = efficiency;
//		//				}
//		//			}
//				}
//		
//				// Find minimal efficiency.
//				min_efficiency = 0;
//				for (int i=0; i<n; ++i) {
//					Int e = cache[i];
//					if (e < min_efficiency) {
//						min_efficiency = e;
//					}
//				}
//		
//				// Scale efficiency to make it positive. Since efficency is stored
//				// as a power of two we can just add min value to every value
//				// in an array so that all values become positive.
//				for (int i=0; i<n; ++i) {
//					cache[i] -= min_efficiency - 1;
//				}
//		
//				// Then we need to scale efficiency to some big number
//				// to increase accuracy.
//				Int sum_efficiency = 0;
//				for (int i=0; i<n; ++i) {
//					sum_efficiency += cache[i];
//				}
//				Int factor = SCALE / sum_efficiency;
//				for (int i=0; i<n; ++i) {
//					cache[i] *= factor;
//				}
//				sum_efficiency *= factor;
//				min_efficiency = cache[0];
//				for (int i=1; i<n; ++i) {
//					Int e = cache[i];
//					if (e < min_efficiency) {
//						min_efficiency = e;
//					}
//				}
//		
//		
//				// count samples lesser than average run time
//				count = upstream.count_samples_lesser_than_average();
//		
//				// The same as step = probability of having a task of average complexity or less * efficiency,
//				// but with rounding to nearest integer.
//				step = min_efficiency * count / num_samples;
//		
//				// Advance cursor one step further.
//				cursor += step;
//				if (cursor > sum_efficiency) {
//					int ratio = cursor / sum_efficiency;
//					cursor -= ratio*sum_efficiency;
//				}
//		
//				// Find server corresponding to cursor.
//				int p = 0;
//				int sum2 = cursor - cache[0];
//				while (sum2 > 0) { sum2 -= cache[++p]; }
//		
//				return p;
//			}
//
//			friend std::ostream& operator<<(std::ostream& out, const Stochastic_round_robin& rhs) {
//				int n = rhs.size;
//				out << ',' << rhs.step;
//				for (int i=0; i<n; ++i) {
//					out << ',' << rhs.cache[i];
//				}
//				out << ',';
//				for (int i=0; i<n; ++i)
//					out << (rhs.is_resetting[i] ? '1' : '0');
//				out << ',' << rhs.min_efficiency;
//				out << ',' << rhs.count;
//				out << ',' << rhs.num_samples;
//				return out;
//			}
//		
//			struct Profiler {
//				virtual std::int64_t sum_dynamic_metric() const = 0;
//				virtual Int dynamic_metric() const = 0;
//				virtual Int static_metric() const = 0;
//				virtual int count_samples_lesser_than_average() const = 0;
//				virtual int num_samples() const = 0;
//				virtual bool is_resetting() const = 0;
//			};
//		
//			struct Iprofiler: public Profiler {
//				
//				explicit Iprofiler(const std::vector<Profiler*>& profilers_): upstream_(profilers_) {}
//		
//				std::int64_t sum_dynamic_metric() const {
//					int n = upstream_.size();
//					std::int64_t sum = 0;
//					for (int i=0; i<n; ++i)
//						sum += upstream_[i]->sum_dynamic_metric();
//					return sum;
//				}
//		
//				Int dynamic_metric() const {
//					return log2(sum_dynamic_metric() / num_samples() / static_metric());
//				}
//		
//				Int static_metric() const { 
//					int n = upstream_.size();
//					Int sum = 0;
//					for (int i=0; i<n; ++i)
//						sum += upstream_[i]->static_metric();
//					return sum;
//				}	
//			
//				int count_samples_lesser_than_average() const {
//					int n = upstream_.size();
//					int sum = 0;
//					for (int i=0; i<n; ++i)
//						sum += upstream_[i]->count_samples_lesser_than_average();
//					return sum;
//				}
//			
//				int num_samples() const {
//					int n = upstream_.size();
//					int sum = 0;
//					for (int i=0; i<n; ++i)
//						sum += upstream_[i]->num_samples();
//					return sum;
//				}
//		
//				bool is_resetting() const {
//					bool b = false;
//					int n = upstream_.size();
//					for (int i=0; i<n; ++i) {
//						if (upstream_[i]->is_resetting()) {
//							b = true;
//							break;
//						}
//					}
//					return b;
//				}
//		
//				Profiler& operator[](int i) { return *upstream_[i]; }
//				int size() const { return upstream_.size(); }
//		
//			private:
//				std::vector<Profiler*> upstream_;
//			};
//
//			template<class A>
//			struct Carrier {
//
//				Carrier(): kernel(nullptr) {}
//				Carrier(A* a): kernel(a), arrival(now()) {}
//
//				void act() { 
//					Time t0 = now();
//					kernel->act();
//					Time t1 = now();
//					action = t1-t0;
//				}
//
//				Time arrival_time() const { return arrival; }
//				Time action_time() const { return action; }
//				A* get_kernel() { return kernel; }
//				const A* get_kernel() const { return kernel; }
//
//			private:
//				A* kernel;
//				Time arrival;
//				Time action;
//			};
//		
//			struct Rprofiler: public Profiler {
//		
//				Rprofiler():
//					next_sample(0),
//					nsamples(max_samples()),
//					next_count(0),
//					ncount(0),
//					atomic_metric(0),
//					atomic_sum_metric(0),
//					atomic_count(nsamples),
//					atomic_num_samples(nsamples)
//				{
//					for (int i=0; i<NUM_SAMPLES; ++i) samples[i] = 0;
//					for (int i=0; i<NUM_COUNT; ++i) count[i] = 0;
//				}
//		
//				void reset() {
//					for (int i=0; i<NUM_SAMPLES; ++i) samples[i] = 0;
//					nsamples = 1;
//					next_sample = 0;
//					for (int i=0; i<NUM_COUNT; ++i) count[i] = 0;
//					next_count = 0;
//					ncount = 0;
//				}
//		
//		
//				// TODO: Number of samples should be adjusted according to derivative of variance.
//				// For example, if half of the samples have greater variance than all the samples 
//				// then number of samples should be increased by a factor of 2. Similarly when 
//				// half of the samples have lesser variance then number of samples should be
//				// decreased by a factor of 2. The factor can be 3 or 4. The adjustment help 
//				// to dump oscillations during preprocessing period when tasks are highly 
//				// heterogeneous.
//			private:
//				// heterogeneous tasks case
//		//		static const int NUM_SAMPLES = 32;
//		//		static const int NUM_COUNT   = 5;
//		//		static const int MULTIPLIER  = 5;
//		
//				// default case
//				static const int NUM_SAMPLES = 8;
//				static const int NUM_COUNT   = 7;
//				static const int MULTIPLIER  = 3;
//				int max_samples() const { return NUM_SAMPLES; }
//		
//			public:
//				std::int64_t sum_dynamic_metric() const { return atomic_sum_metric; }
//				Int dynamic_metric() const { return atomic_metric; }
//				Int static_metric() const { return 1; }
//				int count_samples_lesser_than_average() const { return atomic_count; }
//				int num_samples() const { return atomic_num_samples; }
//				bool is_resetting() const { return num_samples() < 4/*max_samples()*/; }
//		
//				/// Records problem complexity and efficiency of a processor that solved this problem.
//				template<class A>
//				bool operator()(Carrier<A>& carrier) {
//
//					if (!carrier.get_kernel()->is_profiled()) {
//						carrier.act();
//						return false;
//					}
//		
//					Time t0 = now();
//					carrier.act();
//					Time t1 = now();
//					Time sample = log2(t1 - t0);
//					if (sample < 20) return false;
//		
//					// TODO: Adjust maximum number of samples according to confidence interval of mean value.
//					reset_if_needed(sample);
//					collect_sample(sample);
//					collect_count(sample);
//					update_metrics(sample);
//		
//					return true;
//				}
//			
//				friend std::ostream& operator<<(std::ostream& out, const Rprofiler& rhs) {
//					out << rhs.atomic_metric << ','
//					    << -rhs.atomic_metric << ','
//					    << rhs.atomic_count << ','
//					    << rhs.atomic_num_samples << ','
//					    << (rhs.is_resetting() ? '1' : '0') << ',';
//					out << ',';
//					out << ',';
//					for (int i=0; i<NUM_COUNT; ++i)
//						out << rhs.count[i] << ',';
//					out << ',';
//					for (int i=0; i<NUM_SAMPLES; ++i)
//						out << rhs.samples[i] << ',';
//					out << rhs.nsamples;
//					return out;
//				}
//			
//			private:
//		
//				void reset_if_needed(Time sample) {
//					Int m = mean();
//					Int boundary = critical_deviation(m);
//					if (sample > m + boundary || sample < m - boundary) {
//						reset();
//					} else {
//						increment_num_samples();
//					}
//				}
//				void increment_num_samples() { if (nsamples < max_samples()) nsamples++; }
//				void collect_sample(Time sample) {
//					samples[next_sample] = sample;
//					if (++next_sample >= max_samples()) {
//						next_sample = 0;
//					}
//				}
//				void collect_count(Time sample) {
//					int val = count_samples_lesser_than(sample);
//					count[next_count++] = val;
//					if (next_count >= NUM_COUNT) {
//						next_count = 0;
//					}
//					if (ncount < NUM_COUNT) ncount++;
//				}
//		
//				void update_metrics(Time) {
//					std::int64_t sum = calc_sum_dynamic_metric();
//					Int metric = log2(sum / nsamples);
//					atomic_metric = metric;
//					atomic_sum_metric = sum;
//		//			atomic_count = count_samples_lesser_than(metric);
//		//			atomic_count = count_samples_lesser_than(sample);
//					atomic_count = smooth_count();
//					atomic_num_samples = nsamples;
//				}
//		
//				int smooth_count() {
//					int tmp[NUM_COUNT];
//					for (int i=0; i<NUM_COUNT; ++i) tmp[i] = count[i];
//					std::sort(&tmp[0], &tmp[ncount]);
//		//			return tmp[ncount/2];
//					return pow2(log2(tmp[ncount/2]));
//				}
//		
//				Int mean() const {
//					int n = nsamples;
//					Int sum = 0;
//					int m = 0;
//					for (int i=0; i<n; ++i) {
//						Int x = samples[i];
//						if (x != 0) {
//							sum += x;
//							m++;
//						}
//					}
//					return m == 0 ? 0 : sum / m;
//				}
//					
//				/// Average complexity (time) of problems being solved by underlying processor.
//				std::int64_t calc_sum_dynamic_metric() const {
//					int n = nsamples;
//					std::int64_t sum = 0;
//					for (int i=0; i<n; ++i)
//						sum += pow2(samples[i]);
//					return sum;
//				}
//			
//				/// Probability of having problem of complexity lesser than or equal to comp.
//				int count_samples_lesser_than(Int comp) const {
//					int count = 0;
//					for (int i=0; i<nsamples; ++i)
//						if (samples[i] == comp)
//							count++;
//					// be optimistic and stick to the maximum
//		//			if (count == nsamples-1) count++;
//					return count;
//				}
//		
//				Int critical_deviation(Int mean) const {
//					int n = nsamples;
//					Int var = 0;
//					Int m = 0;
//					for (int i=0; i<n; ++i) {
//						Int y = samples[i];
//						if (y != 0) {
//							Int x = y - mean;
//							var += x*x;
//							m++;
//						}
//					}
//					if (m > 1) var /= m - 1;
//					Int std = pow2(log2(var) / 2 + 1/*=log2(MULTIPLIER)*/);
//					if (std == 0) std = MULTIPLIER;
//					return std;
//				}
//		
//			private:
//				Int samples[NUM_SAMPLES];
//				int next_sample;
//				int nsamples;
//				int count[NUM_COUNT];
//				int next_count;
//				int ncount;
//				std::atomic<Int>  atomic_metric;
//				std::atomic<std::int64_t> atomic_sum_metric;
//				std::atomic<int>  atomic_count;
//				std::atomic<int>  atomic_num_samples;
//			};
//
//
//			struct Advisor {
//				template<class A, class S>
//				bool is_ready_to_go(const Carrier<A>& carrier, const S* server, Profiler& profiler) const {
//					Time wait_time = now() - carrier.arrival_time();
//					Time avg = server->upstream_pool().size()*(pow2(profiler.dynamic_metric()));
//					return wait_time > avg && wait_time > 100000000 && avg > 0;
//				}
//			};
//		
//		protected:
//			Int cursor;
//			Int* cache;
//			bool* is_resetting;
//			std::mutex mtx;
//			int size;
//			Int step;
//			int count;
//			int num_samples;
//			Int min_efficiency;
//		
//			static const int SCALE = 1000;
//		};
		
		
		/*
		 #####    ####   #    #  #    #   ####    #####  #####   ######    ##    #    #
		 #    #  #    #  #    #  ##   #  #          #    #    #  #        #  #   ##  ##
		 #    #  #    #  #    #  # #  #   ####      #    #    #  #####   #    #  # ## #
		 #    #  #    #  # ## #  #  # #       #     #    #####   #       ######  #    #
		 #    #  #    #  ##  ##  #   ##  #    #     #    #   #   #       #    #  #    #
		 #####    ####   #    #  #    #   ####      #    #    #  ######  #    #  #    #
		*/
		
//		template<class S1, class S2>
//		struct Combined_strategy {
//
//			template<class Server>
//			struct Strategy: public S1::template Strategy<typename S2::template Strategy<Server>> {
//
//				typedef typename S1::template Strategy<typename S2::template Strategy<Server>> Base1;
//				typedef typename S2::template Strategy<Server> Base2;
//				using typename Base1::send;
//				using typename Base2::send;
//
//			};
//
//			template<class Server>
//			struct Rprofiler: public S1::template Rprofiler<typename S2::template Rprofiler<Server>> {
//			};
//
//			template<class Server>
//			struct Iprofiler: public S1::template Rprofiler<typename S2::template Iprofiler<Server>> {};
//
//		};
		
		template<class Kernel>
		struct Simple_hashing {

			template<class Server>
			struct Strategy: public Server {

//				using Server::send;

//				typedef typename Server::Kernel Kernel;

				void send(Kernel* pair) {
					Logger(Level::STRATEGY) << "Simple_hashing::send()" << std::endl;
					size_t n = Server::_upstream.size();
					if (n > 0) {
						size_t i = std::size_t(pair->principal()) / ALIGNMENT * PRIME % n;
						Server::_upstream[i]->send(pair);
					} else {
						std::cerr << "FATAL. Deleting kernel because there are no upstream servers." << std::endl;
						delete pair;
					}
				}
			};

			template<class Server>
			struct Rprofiler: public Server {
				void process_kernel(Kernel* kernel) {
					Logger(Level::STRATEGY) << "Simple_hashing::process_kernel()" << std::endl;
                    kernel->run_act();
				}
			};

			template<class Server>
			struct Iprofiler: public Server {};


		private:
			static const size_t ALIGNMENT = 64;
			static const size_t PRIME     = 7;
		};

		// remote

		template<class Top>
		struct Resource_aware: public Top {

			typedef Resource_aware<Top> This;

			typedef int Index;

			template<class S>
			explicit Resource_aware(const S& s): Top(s) {}

			template<class K, class I>
			int operator()(K* kernel, I& upstream) {
				return kernel->resource() == "" 
					? Top::operator()(kernel, upstream)
					: send_to_resource(kernel);
			}

			template<class K>
			int send_to_resource(K* kernel) {
				const Endpoint* endp = Resources::resources().lookup(kernel->resource());
				if (endp == nullptr) {
					std::stringstream msg;
					msg << "Can not find server which provides resource '" << kernel->resource() << "'.";
					throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
				}
//				Remote_server<K, This> srv(*endp);
//				srv.send(kernel);
				return -1;
			}

//			template<class K, class I>
//			int operator()(Kernel_pair<K>* pair, I&) {
////				Endpoint endp = pair->subordinate()->from();
////				Remote_server<Kernel_pair<K>, This> srv(endp);
////				srv.send(pair);
//				return -1;
//			}

			struct Profiler: public virtual Top::Profiler {
				Index index() const { return _index; }
				void index(Index i) { _index = i; }
				void endpoint(const Endpoint& endp) { _endpoint = endp; }
				const Endpoint& endpoint() const { return _endpoint; }
			private:
				Index _index;
				Endpoint _endpoint;
			};

			struct Rprofiler: public Profiler, public Top::Rprofiler {};

			struct Iprofiler: public Profiler, public Top::Iprofiler {

				template<class Upstream>
				explicit Iprofiler(Upstream upstream):
					Top::Iprofiler(upstream),
					_resources(create_resource_map(upstream))
				{}

				Index server(const Resource& res) const {
					auto result = _resources.find(res);
					if (result == _resources.end()) {
						std::stringstream msg;
						msg << "Can not find server which provides resource '" << res << "'.";
						throw Durability_error(msg.str(), __FILE__, __LINE__, __func__);
					}
					return result->second;
				}

			private:

				template<class Upstream>
				std::unordered_map<Resource, Index> create_resource_map(Upstream upstream) {
					std::unordered_map<Resource, Index> resources;
					auto resource_map = Resources::resources().map();
					std::for_each(resource_map.cbegin(), resource_map.cend(),
						[&resources, &upstream] (const typename decltype(resource_map)::value_type& pair) {
//			                Logger(Level::STRATEGY) << pair.first << " -> " << *pair.second << std::endl;
							auto result = upstream.find(*pair.second);
							if (result != upstream.cend()) {
								resources[pair.first] = result->second->index();
							}
						}
					);
		            for (auto it=resources.cbegin(); it!=resources.cend(); ++it) {
		                Logger(Level::STRATEGY) << it->first << " -> " << it->second << std::endl;
		            }
					return resources;
				}

				std::unordered_map<Resource, Index> _resources;
			};

			template<class K>
			using Carrier = typename Top::template Carrier<K>;

//			template<class K>
//			struct Carrier: public Top::Carrier<K> {
//				explicit Carrier(K* k): Top::Carrier(k) {}
//			};

		private:
			static const std::size_t ALIGNMENT = 64;
			static const std::size_t PRIME     = 7;
		};

//		struct No_strategy {
//
//			template<class Server>
//			class Strategy: public Server {};
//
//			template<class Server>
//			struct Iprofiler: public Server {};
//
//			template<class Server>
//			struct Rprofiler: public Server {
//				void process_kernel(Kernel* kernel) {
//					kernel->run_act();
//				}
//			};
//
//		};
	
	}

}
