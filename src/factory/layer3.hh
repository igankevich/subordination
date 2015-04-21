namespace factory {

	Factory __factory;

	Local_server* the_server() { return __factory.local_server(); }
	Remote_server* remote_server() { return __factory.remote_server(); }
	External_server* ext_server() { return __factory.ext_server(); }
	Repository_stack* repository() { return __factory.repository(); }

	namespace components {

		void factory_stop() { __factory.stop(); }

		template<class K>
		void factory_send(K* kernel) { the_server()->send(kernel); }

		void factory_server_addr(std::ostream& out) {
			out << remote_server()->server_addr();
		}
	}

	void emergency_shutdown(int) {
		__factory.stop();
		static int num_calls = 0;
		static const int MAX_CALLS = 3;
		num_calls++;
		std::clog << "Ctrl-C shutdown." << std::endl;
		if (num_calls >= MAX_CALLS) {
			std::clog << "MAX_CALLS reached. Aborting." << std::endl;
			std::abort();
		}
	}

}
namespace factory {

	using namespace factory::configuration;

	class Notification: public Kernel {};

	template<class F, class G, class I>
	class Map: public Kernel {
	public:
		Map(F f_, G g_, I a_, I b_, I bs_=1):
			f(f_), g(g_), a(a_), b(b_), bs(bs_), n(0), m(calc_m()) {}
	
		bool is_profiled() const { return false; }
	
		struct Worker: public factory::Kernel {
			Worker(F& f_, I a_, I b_):
				f(f_), a(a_), b(b_) {}
			void act() {
				for (I i=a; i<b; ++i) f(i);
				commit(the_server());
			}
			F& f;
			I a, b;
		};
	
		void act() {
			for (I i=a; i<b; i+=bs) upstream(the_server(), new Worker(f, i, std::min(i+bs, b)));
		}
	
		void react(factory::Kernel* kernel) {
			Worker* w = reinterpret_cast<Worker*>(kernel);
			I x1 = w->a, x2 = w->b;
			for (I i=x1; i<x2; ++i) g(i);
			if (++n == m) commit(the_server());
		}
	
	private:
		I calc_m() const { return (b-a)/bs + ((b-a)%bs == 0 ? 0 : 1); }
	
	private:
		F f;
		G g;
		I a, b, bs, n, m;
	};
	
	
	template<class F, class G, class I>
	Map<F, G, I>* mapreduce(F f, G g, I a, I b, I bs=1) {
		return new Map<F, G, I>(f, g, a, b, bs);
	}

}
