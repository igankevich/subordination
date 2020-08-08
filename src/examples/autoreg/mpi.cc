#include <ostream>
#include <sstream>

#include <autoreg/mpi.hh>

int autoreg::mpi::rank = 0;
int autoreg::mpi::nranks = 0;
char autoreg::mpi::name[MPI_MAX_PROCESSOR_NAME] {};

autoreg::mpi::guard::guard(int* argc, char*** argv) {
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    int unused = 0;
    MPI_Get_processor_name(name, &unused);
}

autoreg::mpi::guard::~guard() {
    MPI_Finalize();
}

std::string autoreg::mpi::to_string(errc rhs) {
    std::string tmp(MPI_MAX_ERROR_STRING, '\0');
    int size = 0;
    MPI_Error_string(int(rhs), &tmp[0], &size);
    tmp.resize(size);
    return tmp;
}
std::ostream&
autoreg::mpi::operator<<(std::ostream& out, errc rhs) {
    const auto& s = to_string(rhs);
    if (!s.empty()) { out << s; }
    else { out << "unknown(" << int(rhs) << ')'; }
    return out;
}

autoreg::mpi::error_category autoreg::mpi::mpi_category;

std::string
autoreg::mpi::error_category::message(int ev) const noexcept {
    auto cond = static_cast<errc>(ev);
    const auto& str = to_string(cond);
    if (!str.empty()) { return str; }
    std::stringstream msg;
    msg << cond;
    return msg.str();
}
