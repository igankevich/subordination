#ifndef APPS_AUTOREG_MAPREDUCE_HH
#define APPS_AUTOREG_MAPREDUCE_HH

#include <factory/api.hh>

namespace factory {

	namespace api {

		class Notification: public Kernel {};

		template<class F, class G, class I>
		struct Map: public Kernel {

			Map(F f_, G g_, I a_, I b_, I bs_=1):
			f(f_), g(g_), a(a_), b(b_), bs(bs_), n(0), m(calc_m()) {}

			struct Worker: public Kernel {
				Worker(F& f_, I a_, I b_):
				f(f_), a(a_), b(b_) {}
				void
				act() {
					for (I i=a; i<b; ++i) f(i);
					commit<Local>(this);
				}

				F& f;
				I a, b;
			};

			void
			act() {
				for (I i=a; i<b; i+=bs) {
					upstream<Local>(this, new Worker(f, i, std::min(i+bs, b)));
				}
			}

			void
			react(Kernel* kernel) {
				Worker* w = dynamic_cast<Worker*>(kernel);
				I x1 = w->a, x2 = w->b;
				for (I i=x1; i<x2; ++i) g(i);
				if (++n == m) commit<Local>(this);
			}

		private:

			I
			calc_m() const {
				return (b-a)/bs + ((b-a)%bs == 0 ? 0 : 1);
			}

			F f;
			G g;
			I a, b, bs, n, m;

		};

		template<class F, class G, class I>
		Map<F, G, I>*
		mapreduce(F f, G g, I a, I b, I bs=1) {
			return new Map<F, G, I>(f, g, a, b, bs);
		}

	}

}

#endif // vim:filetype=cpp
