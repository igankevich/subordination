#include <algorithm>
#include <iterator>
#include <string>

#include <subordination/base/queue_popper.hh>
#include <subordination/base/queue_pusher.hh>

/**
\example queue_pusher_and_popper.cc
\details
Push all command line arguments to a queue,
pop each element and print it to stdout.
*/
int main(int arch, char* argv[]) {
    std::queue<std::string> q;
    /// [Push elements]
    std::copy(
        sys::cstring_iterator<char*>(argv),
        sys::cstring_iterator<char*>(),
        sbn::queue_pusher(q)
    );
    /// [Push elements]
    /// [Pop elements]
    std::copy(
        sbn::queue_popper(q),
        sbn::queue_popper(),
        std::ostream_iterator<std::string>(std::cout, "\n")
    );
    /// [Pop elements]
    return 0;
}
