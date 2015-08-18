#ifndef FACTORY_SERVER_CPU_SERVER_HH
#define FACTORY_SERVER_CPU_SERVER_HH

namespace factory {

	namespace components {

		template<class T>
		struct CPU_server: public Standard_server_with_pool<T> {

			typedef Standard_server_with_pool<T> base_server;
			using typename base_server::kernel_type;
			using typename base_server::lock_type;

			CPU_server(CPU_server&& rhs) noexcept:
			base_server(std::move(rhs))
			{}

			CPU_server() noexcept:
			base_server(std::thread::hardware_concurrency())
			{}
				
			CPU_server(const CPU_server&) = delete;
			CPU_server& operator=(const CPU_server&) = delete;
			~CPU_server() = default;

			friend std::ostream&
			operator<<(std::ostream& out, const CPU_server& rhs) {
				return out << "cpuserver";
			}

			void add_cpu(size_t) {}

		private:

			void
			do_run() override {
				while (!this->stopped()) {
					lock_type lock(this->_mutex);
					this->_semaphore.wait(lock, [this] () {
						return this->stopped() || !this->_kernels.empty();
					});
					if (!this->stopped()) {
						kernel_type* kernel = stdx::front(this->_kernels);
						stdx::pop(this->_kernels);
						lock.unlock();
						kernel->run_act();
					}
				}
			}
			
		};

	}

}
#endif // FACTORY_SERVER_CPU_SERVER_HH
