#ifndef SUBORDINATION_BASE_STATIC_LOCK_HH
#define SUBORDINATION_BASE_STATIC_LOCK_HH

namespace bsc {

    template <class M1, class M2=M1>
    class static_lock {

    private:
        M1* _m1;
        M2* _m2;

    public:

        inline
        static_lock(M1* a, M2* b) {
            if (a < b || !b) {
                this->_m1 = a;
                this->_m2 = b;
            } else {
                this->_m1 = b;
                this->_m2 = a;
            }
            this->lock();
        }

        inline
        ~static_lock() {
            this->unlock();
        }

        inline void
        lock() {
            this->_m1->lock();
            if (this->_m2) {
                this->_m2->lock();
            }
        }

        inline void
        unlock() {
            if (this->_m2) {
                this->_m2->unlock();
            }
            this->_m1->unlock();
        }

    };

}

#endif // vim:filetype=cpp
