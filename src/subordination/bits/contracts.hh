#ifndef SUBORDINATION_BITS_CONTRACTS_HH
#define SUBORDINATION_BITS_CONTRACTS_HH

#if defined(SBN_DEBUG)

#include <unistdx/base/contracts>
#define Expects(cond) UNISTDX_PRECONDITION(cond)
#define Ensures(cond) UNISTDX_POSTCONDITION(cond)
#define Assert(cond) UNISTDX_ASSERTION(cond)

#else

#define Expects(cond)
#define Ensures(cond)
#define Assert(cond)

#endif

#endif // vim:filetype=cpp
