#include <autoreg/mpi.hh>

int autoreg::mpi::rank = 0;
int autoreg::mpi::nranks = 0;

autoreg::mpi::guard::guard(int* argc, char*** argv) {
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
}

autoreg::mpi::guard::~guard() {
    MPI_Finalize();
}
