#ifndef EXAMPLES_CPP_AUTOREG_MAPREDUCE_HH
#define EXAMPLES_CPP_AUTOREG_MAPREDUCE_HH

#include <autoreg/api.hh>

namespace sbn {

    class Notification: public kernel {};

    template<class F, class G, class I>
    struct Map: public kernel {

        Map(F f_, G g_, I a_, I b_, I bs_=1):
        f(f_), g(g_), a(a_), b(b_), bs(bs_), n(0), m(calc_m()) {}

        struct Worker: public kernel {
            Worker(F& f_, I a_, I b_):
            f(f_), a(a_), b(b_) {}
            void
            act() {
                for (I i=a; i<b; ++i) f(i);
                commit<Local>(std::move(this_ptr()));
            }

            F& f;
            I a, b;
        };

        void
        act() override {
            for (I i=a; i<b; i+=bs) {
                upstream<Local>(this, sbn::make_pointer<Worker>(f, i, std::min(i+bs, b)));
            }
        }

        void
        react(kernel_ptr&& k) override {
            auto w = sbn::pointer_dynamic_cast<Worker>(std::move(k));
            I x1 = w->a, x2 = w->b;
            for (I i=x1; i<x2; ++i) g(i);
            if (++n == m) { commit<Local>(std::move(this_ptr())); }
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
    sbn::pointer<Map<F, G, I>>
    mapreduce(F f, G g, I a, I b, I bs=1) {
        return sbn::make_pointer<Map<F, G, I>>(f, g, a, b, bs);
    }

}

#endif // vim:filetype=cpp
