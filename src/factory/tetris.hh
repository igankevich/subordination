namespace factory {

	namespace components {

		class Tetris {
			typedef std::int64_t Int;
		public:
			template<class S>
			explicit Tetris(const S& upstream):
				cache(new Int[upstream.size()]),
				mtx(),
				size(upstream.size())
			{}

			Tetris(const Tetris& rhs) = delete;
		
			~Tetris() { delete[] cache; }
		
			template<class A, class I>
			int operator()(A*, I& upstream) {
		
				int n = upstream.size();

				std::lock_guard<Spin_mutex> lock(mtx);

				Int max_load = std::numeric_limits<Int>::min();
				for (int i=0; i<n; ++i) {
					Int x = upstream[i].load();
					if (x > max_load) {
						max_load = x;
					}
					cache[i] = x;
				}

				Int pred_metric = upstream.dynamic_metric();
				if (pred_metric == 0) {
					pred_metric = 1;
				}

				int sr1 = -1;
				int sr2 = -1;
				Int obj1 = std::numeric_limits<Int>::max();
				Int obj2 = std::numeric_limits<Int>::min();

				for (int i=0; i<n; ++i) {
					Int obj = max_load - cache[i] - pred_metric;
					if (obj >= 0) {
						if (obj < obj1) {
							sr1 = i;
							obj1 = obj;
						}
					} else {
						if (obj > obj2) {
							sr2 = i;
							obj2 = obj;
						}
					}
				}

				this->predicted_metric = pred_metric;
				this->srv1 = sr1;
				this->srv2 = sr2;

				int server = sr1 == -1 ? sr2 : sr1;
				upstream[server].increase_load(pred_metric);
				return server;
			}

			friend std::ostream& operator<<(std::ostream& out, const Tetris& rhs) {
				int n = rhs.size;
				for (int i=0; i<n; ++i) {
					out << ',' << rhs.cache[i];
				}
				out << ',' << rhs.predicted_metric;
				out << ',' << rhs.srv1;
				out << ',' << rhs.srv2;
				return out;
			}
		
			struct Profiler {
				virtual std::int64_t sum_dynamic_metric() const = 0;
				virtual Int dynamic_metric() const = 0;
				virtual Int static_metric() const = 0;
				virtual int num_samples() const = 0;
				virtual void increase_load(Int) = 0;
				virtual Int load() const = 0;
				virtual Int sum_load() const = 0;
			};
		
			struct Iprofiler: public Profiler {
				
				explicit Iprofiler(const std::vector<Profiler*>& profilers_): upstream_(profilers_) {}

				void increase_load(Int) {}

				Int sum_load() const {
					size_t n = upstream_.size();
					Int sum = 0;
					for (size_t i=0; i<n; ++i)
						sum += upstream_[i]->sum_load();
					return sum;
				}

				Int load() const { return sum_load() / static_metric(); }
		
				std::int64_t sum_dynamic_metric() const {
					size_t n = upstream_.size();
					int64_t sum = 0;
					for (size_t i=0; i<n; ++i)
						sum += upstream_[i]->sum_dynamic_metric();
					return sum;
				}
		
				Int dynamic_metric() const {
					return sum_dynamic_metric() / num_samples();// / static_metric();
				}
		
				Int static_metric() const { 
					size_t n = upstream_.size();
					Int sum = 0;
					for (size_t i=0; i<n; ++i)
						if (upstream_[i]->num_samples() > 0)
							sum += upstream_[i]->static_metric();
					return sum == 0 ? 1 : sum;
				}	
			
				int num_samples() const {
					size_t n = upstream_.size();
					int sum = 0;
					for (size_t i=0; i<n; ++i)
						sum += upstream_[i]->num_samples();
					return sum == 0 ? 1 : sum;
				}
		
				Profiler& operator[](size_t i) { return *upstream_[i]; }
				size_t size() const { return upstream_.size(); }
		
			private:
				std::vector<Profiler*> upstream_;
			};

			template<class A>
			struct Carrier {

				Carrier(): kernel(nullptr) {}
				Carrier(A* a): kernel(a), arrival(current_time_nano()) {}

				void act() { 
					Time t0 = current_time_nano();
					kernel->act();
					Time t1 = current_time_nano();
					action = t1-t0;
				}

				Time arrival_time() const { return arrival; }
				Time action_time() const { return action; }
				A* get_kernel() { return kernel; }
				const A* get_kernel() const { return kernel; }

			private:
				A* kernel;
				Time arrival;
				Time action;
			};
		
			struct Rprofiler: public Profiler {
		
				Rprofiler():
					next_sample(0),
					nsamples(0),
					atomic_load(0),
					atomic_metric(0),
					atomic_sum_metric(0),
					atomic_num_samples(0)
				{
					for (int i=0; i<NUM_SAMPLES; ++i) samples[i] = 0;
				}
		
			private:
				static const int NUM_SAMPLES = 4;
		
			public:
				std::int64_t sum_dynamic_metric() const { return atomic_sum_metric; }
				Int dynamic_metric() const { return atomic_metric; }
				Int static_metric() const { return 1; }
				int num_samples() const { return atomic_num_samples; }
				Int sum_load() const { return atomic_load; }
				Int load() const { return sum_load(); }
				void increase_load(Int x) { atomic_load += x; }
		
				template<class A>
				bool operator()(Carrier<A>& carrier) {

					if (!carrier.get_kernel()->is_profiled()) {
						carrier.act();
						return false;
					}
		
					Time t0 = current_time_nano();
					carrier.act();
					Time t1 = current_time_nano();
					Time sample = (t1 - t0);
		
					collect_sample(sample);
					update_metrics(sample);
		
					return true;
				}
			
				friend std::ostream& operator<<(std::ostream& out, const Rprofiler& rhs) {
					out << rhs.atomic_load << ','
					    << rhs.atomic_metric << ','
					    << rhs.atomic_num_samples << ','
					    << ',';
					for (int i=0; i<NUM_SAMPLES; ++i)
						out << rhs.samples[i] << ',';
					out << rhs.nsamples;
					return out;
				}
			
			private:
		
				void collect_sample(Time sample) {
					samples[next_sample] = sample;
					if (++next_sample >= NUM_SAMPLES) {
						next_sample = 0;
					}
					if (nsamples < NUM_SAMPLES) {
						nsamples++; 
					}
				}
		
				void update_metrics(Time sample) {
					std::int64_t sum = calc_sum_dynamic_metric();
					Int metric = sum / nsamples;
					if (atomic_load < sample) {
//						atomic_load--;
						atomic_load = 0;
					} else {
						atomic_load -= sample;
					}
					atomic_metric = metric;
					atomic_sum_metric = sum;
					atomic_num_samples = nsamples;
				}
		
				std::int64_t calc_sum_dynamic_metric() const {
					int n = nsamples;
					std::int64_t sum = 0;
					for (int i=0; i<n; ++i)
						sum += samples[i];
					return sum;
				}
			
			private:
				Int samples[NUM_SAMPLES];
				int next_sample;
				int nsamples;
				std::atomic<Int> atomic_load;
				std::atomic<Int> atomic_metric;
				std::atomic<std::int64_t> atomic_sum_metric;
				std::atomic<int> atomic_num_samples;
			};


			struct Advisor {
				template<class A, class S>
				bool is_ready_to_go(const Carrier<A>& carrier, const S* server, Profiler& profiler) const {
					Time wait_time = current_time_nano() - carrier.arrival_time();
					Time avg = server->upstream_pool().size()*(profiler.dynamic_metric());
					return wait_time > avg && wait_time > 100000000 && avg > 0;
				}
			};
		
		protected:
			Int* cache;
			Spin_mutex mtx;
			int size;
			Int predicted_metric;
			int srv1 = -1;
			int srv2 = -1;
		};

	}

}
