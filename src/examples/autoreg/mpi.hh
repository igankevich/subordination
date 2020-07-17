#ifndef EXAMPLES_AUTOREG_MPI_HH
#define EXAMPLES_AUTOREG_MPI_HH

#if defined(AUTOREG_MPI)

#include <mpi.h>

#include <utility>

namespace autoreg {

    namespace mpi {

        extern int rank, nranks;

        struct guard {
            guard(int* argc, char*** argv);
            ~guard();
        };

        template <class T> struct type;

        #define AUTOREG_MPI_TYPE(a,b) \
            template <> struct type<a> { static constexpr const auto value = b; }

        AUTOREG_MPI_TYPE(char, MPI_CHAR);
        AUTOREG_MPI_TYPE(signed char, MPI_CHAR);
        AUTOREG_MPI_TYPE(signed short int, MPI_SHORT);
        AUTOREG_MPI_TYPE(signed int, MPI_INT);
        AUTOREG_MPI_TYPE(signed long int, MPI_LONG);
        AUTOREG_MPI_TYPE(unsigned char, MPI_UNSIGNED_CHAR);
        AUTOREG_MPI_TYPE(unsigned short int, MPI_UNSIGNED_SHORT);
        AUTOREG_MPI_TYPE(unsigned int, MPI_UNSIGNED);
        AUTOREG_MPI_TYPE(unsigned long int, MPI_UNSIGNED_LONG);
        AUTOREG_MPI_TYPE(float, MPI_FLOAT);
        AUTOREG_MPI_TYPE(double, MPI_DOUBLE);
        AUTOREG_MPI_TYPE(long double, MPI_LONG_DOUBLE);

        #undef AUTOREG_MPI_TYPE

        class status: public ::MPI_Status {

        public:
            inline int source() const noexcept { return this->MPI_SOURCE; }
            inline int tag() const noexcept { return this->MPI_TAG; }
            inline int error() const noexcept { return this->MPI_ERROR; }

            template <class T> inline int
            count() const {
                int n = 0;
                MPI_Get_count(this, type<T>::value, &n);
                return n;
            }

            template <class T> inline int
            num_elements() const {
                int n = 0;
                MPI_Get_elements(this, type<T>::value, &n);
                return n;
            }

            inline bool cancelled() const {
                int flag = 0;
                MPI_Test_cancelled(this, &flag);
                return flag;
            }

        };

        template <class T> inline void
        broadcast(T* data, int size, int root=0, MPI_Comm comm=MPI_COMM_WORLD) {
            MPI_Bcast(data, size, type<T>::value, root, comm);
        }

        template <class T> inline void
        send(T* data, int size, int destination, int tag=MPI_ANY_TAG,
             MPI_Comm comm=MPI_COMM_WORLD) {
            MPI_Send(data, size, type<T>::value, destination, tag, comm);
        }

        template <class T> inline void
        receive(T* data, int size, int source=MPI_ANY_SOURCE, int tag=MPI_ANY_TAG,
                MPI_Comm comm=MPI_COMM_WORLD,
                status* s=static_cast<status*>(MPI_STATUS_IGNORE)) {
            MPI_Recv(data, size, type<T>::value, source, tag, comm, s);
        }

        inline status
        probe(int source=MPI_ANY_SOURCE, int tag=MPI_ANY_TAG, MPI_Comm comm=MPI_COMM_WORLD) {
            status s;
            MPI_Probe(source, tag, comm, &s);
            return s;
        }

        class request {

        private:
            MPI_Request _request = MPI_REQUEST_NULL;

        public:
            request() = default;
            request(const request&) = delete;
            request& operator=(const request&) = delete;

            inline request(request&& rhs) noexcept: _request(rhs._request) {
                rhs._request = MPI_REQUEST_NULL;
            }

            inline request& operator=(request&& rhs) noexcept {
                swap(rhs);
                return *this;
            }

            inline void swap(request& rhs) {
                std::swap(this->_request, rhs._request);
            }

            inline ~request() {
                if (this->_request != MPI_REQUEST_NULL) {
                    MPI_Request_free(&this->_request);
                }
            }

            inline void cancel() { MPI_Cancel(&this->_request); }
            inline operator MPI_Request*() { return &this->_request; }

            inline status wait() {
                status s;
                MPI_Wait(&this->_request, &s);
                return s;
            }

            inline std::pair<status,bool> test() {
                status s;
                int flag = 0;
                MPI_Test(&this->_request, &flag, &s);
                return std::make_pair(s,flag);
            }

            inline bool completed() {
                int flag = 0;
                MPI_Test(&this->_request, &flag, MPI_STATUS_IGNORE);
                return flag;
            }

        };

        inline void swap(request& a, request& b) { a.swap(b); }

        template <class T> inline request
        async_send(T* data, int size, int destination, int tag=MPI_ANY_TAG,
                   MPI_Comm comm=MPI_COMM_WORLD) {
            request r;
            MPI_Isend(data, size, type<T>::value, destination, tag, comm, r);
            return r;
        }

        template <class T> inline request
        async_receive(T* data, int size, int source=MPI_ANY_SOURCE, int tag=MPI_ANY_TAG,
                      MPI_Comm comm=MPI_COMM_WORLD,
                      status* s=static_cast<status*>(MPI_STATUS_IGNORE)) {
            request r;
            MPI_Irecv(data, size, type<T>::value, source, tag, comm, s, r);
            return r;
        }

        inline std::pair<status,bool>
        async_probe(int source=MPI_ANY_SOURCE, int tag=MPI_ANY_TAG,
                    MPI_Comm comm=MPI_COMM_WORLD) {
            status s;
            int success = 0;
            MPI_Iprobe(source, tag, comm, &success, &s);
            return std::make_pair(s,success);
        }

    }

}

#define AUTOREG_MPI_SUPERIOR(...) \
    if (::autoreg::mpi::rank == 0) { \
        __VA_ARGS__ \
    }
#define AUTOREG_MPI_SUBORDINATE(...) \
    if (::autoreg::mpi::rank != 0) { \
        __VA_ARGS__ \
    }
#else
#define AUTOREG_MPI_SUPERIOR(...) __VA_ARGS__
#define AUTOREG_MPI_SUBORDINATE(...) __VA_ARGS__
#endif

#endif // vim:filetype=cpp
