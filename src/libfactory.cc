#include <factory/factory.hh>

namespace factory {
	namespace components {
		Underflow underflow;
		End_packet end_packet;
	}
}

namespace factory {
	Factory __factory;

	namespace components {
		Id factory_start_id() {
			struct factory_start_id_t{};
			typedef stdx::log<factory_start_id_t> this_log;
			constexpr static const Id DEFAULT_START_ID = 1000;
			Id i = unix::this_process::getenv("START_ID", DEFAULT_START_ID);
			if (i == ROOT_ID) {
				i = DEFAULT_START_ID;
				this_log() << "Bad START_ID value: " << ROOT_ID << std::endl;
			}
			this_log() << "START_ID = " << i << std::endl;
			return i;
		}

		Id factory_generate_id() {
			static std::atomic<Id> counter(factory_start_id());
			return counter++;
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
//		std::random_device rng;
	}
}
