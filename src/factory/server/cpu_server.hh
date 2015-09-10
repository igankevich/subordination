#ifndef FACTORY_SERVER_CPU_SERVER_HH
#define FACTORY_SERVER_CPU_SERVER_HH

#include <stdx/unlock_guard.hh>
#include <factory/server/intro.hh>
#include <factory/managed_object.hh>

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

			void add_cpu(size_t) {}

			Category
			category() const noexcept override {
				return Category{
					"cpu_server",
					[] () { return new CPU_server; },
					{"nthreads"},
					[] (const void* obj, Category::key_type key) {
						const CPU_server* lhs = static_cast<const CPU_server*>(obj);
						if (key == "nthreads") {
							return std::to_string(lhs->_threads.size());
						}
						return Category::value_type();
					}
				};
			}

		private:

			void
			do_run() override {
				lock_type lock(this->_mutex);
				this->_semaphore.wait(lock, [this,&lock] () {
					while (!this->_kernels.empty()) {
						kernel_type* kernel = stdx::front(this->_kernels);
						stdx::pop(this->_kernels);
						stdx::unlock_guard<lock_type> g(lock);
						kernel->run_act(*this);
					}
					return this->stopped();
				});
			}
			
		};

	}

}

namespace stdx {

	template<class T>
	struct type_traits<factory::components::CPU_server<T>> {
		static constexpr const char*
		short_name() { return "cpu_server"; }
		typedef factory::components::server_category category;
	};

}

#endif // FACTORY_SERVER_CPU_SERVER_HH
