\page tutorial Tutorial

\section intro Introduction

Every Factory application is composed of computational \link factory::Kernel <em>
kernels</em>\endlink --- self-contained objects which store data and have routines
to process it. In each routine a kernel may create any number of \em
subordinates to decompose application into smaller parts. Some kernels can be
transferred to another cluster node to make application distributed. An
application exits when there are no kernels left to process.

Kernels are processed by \link factory::Server_with_pool <em>pipelines</em>\endlink
--- kernel queues with processing threads attached. Each device has its own
pipeline (there is a pipeline for CPU, I/O device and NIC) which allows them to work
in parallel: process one part of data with CPU pipeline and simultaneously write
another one with disk pipeline. Every pipeline work until application exit.

Each programme begins with starting all necessary pipelines and sending the main
kernel to one of the them. After that programme execution resembles that of
sequential programme with each nested call to a procedure replaced with
construction of a subordinate and sending it to appropriate pipeline. The
difference is that pipelines process kernels \em asynchronously, so procedure
code is decomposed into \link factory::Kernel::act() `act()`\endlink routine
which constructs subordinates and \link factory::Kernel::react(Kernel*)
`react()`\endlink routine which processes results they return.

\section api Developing distributed applications

The first step is to decide which pipelines your programme needs. Most probably
these are standard
- \link factory::CPU_server CPU pipeline\endlink,
- \link factory::IO_server I/O pipeline\endlink,
- \link factory::NIC_server NIC pipeline\endlink and
- \link factory::Timer_server Timer pipeline\endlink (for periodic and
  schedule-based execution of kernels).

Standard pipelines for all devices except NIC are initialised in
`factory/factory.hh` header. To initialise NIC pipeline you need to tell it
which pipeline is local and which one is remote. The following code snippet
shows the usual way of doing this.

\code{.cpp}
#include <factory/factory.hh>
#include <factory/cpu_server.hh>
#include <factory/timer_server.hh>
#include <factory/nic_server.hh>
#include <factory/kernel.hh>

namespace factory {

	struct Router {

		void
		send_local(Kernel* rhs) {
			local_server.send(rhs);
		}

		void
		send_remote(Kernel*);

		void
		forward(const Kernel_header& hdr, sys::packetstream& istr) {
			assert(false);
		}

	};

	NIC_server<Kernel, sys::socket, Router> remote_server;

	void
	Router::send_remote(Kernel* rhs) {
		remote_server.send(rhs);
	}

}
\endcode

The second step is to subclass \link factory::Kernel `factory::Kernel`\endlink
and implement `act()` and `react()` member functions for each sequential stage of
your programme and for parallel parts of each stage.

\code{.cpp}
struct My_app: public factory::Kernel {

	void
	act() {
		std::cout << "Hello world" << std::endl;
		factory::commit(local_server, this);
	}

	void
	react(factory::Kernel*) {
		// not needed for such a simple programme
	}

};
\endcode

\note Even if your programme does not have parallel parts, its performance may
still increase by constructing a separate instance of a kernel for each input
file. For example, to make a programme that searches files for matching pattern
(`grep` command in UNIX) parallel it is sufficient to construct a kernel for
each file and send all of them to CPU pipeline. A better way is to construct
a separate kernel to read portions of the files via I/O pipeline and for each
portion construct and send new kernel to CPU pipeline to process it in parallel.

Finally, you need to start every pipeline and send the main kernel to the local
one via \link factory::upstream\endlink function.

\code{.cpp}
int main() {
	using factory::local_server;
	using factory::remote_server;
	factory::Terminate_guard g0;
	factory::Server_guard<decltype(local_server)> g1(local_server);
	factory::Server_guard<decltype(remote_server)> g2(remote_server);
	factory::upstream(local_server, nullptr, new My_app);
	return factory::wait_and_return();
}
\endcode

Use \link factory::commit\endlink to return the kernel to its parent and
reclaim system resources.

\section architecture Framework architecture

\section bottomup Bottom-up design

Factory framework uses [bottom-up](http://www.paulgraham.com/progbot.html)
source code development approach which means we create low-level abstractions
to simplify high-level code and make it clean and readable. There are two
layers of abstractions:

- \link sys operating system layer\endlink, which contains thin C++ wrappers
  for POSIX and Linux system calls and data structures, and
- \link stdx generic C++ abstractions layer\endlink, which consists of template
  routines and classes (e.g. iterators, I/O stream manipulators, guard objects
  etc.) that are missing in STL.

From developer perspective it is unclear which layer is the bottom one, but
still we would like to separate them via
[policy-based programming](https://erdani.com/publications/xp2000.pdf) and
[traits classes](http://erdani.com/publications/traits.html).

It is easy to extend Factory source code base: just put all system-related
abstractions into `sys` directory, all generic structures and functions into
`stdx` directory and all framework-specific abstractions into `factory`
directory.
