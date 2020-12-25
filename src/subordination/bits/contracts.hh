#ifndef SUBORDINATION_BITS_CONTRACTS_HH
#define SUBORDINATION_BITS_CONTRACTS_HH

#include <unistdx/base/contracts>

#define Expects(cond) UNISTDX_PRECONDITION(cond)
#define Ensures(cond) UNISTDX_POSTCONDITION(cond)
#define Assert(cond) UNISTDX_ASSERTION(cond)

#endif // vim:filetype=cpp
