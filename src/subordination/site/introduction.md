\page introduction Introduction
\ingroup tutorials

\section intro Introduction

Every Subordination application is composed of computational \link sbn::kernel
<em> kernels</em>\endlink --- self-contained objects which store the data, the
routines to process it and the result of the processing. In each routine a
kernel may create any number of child kernels to decompose application into
smaller parts. Some kernels can be transferred to another cluster node to make
application distributed. An application exits when there are no kernels left to
process.

Kernels are processed via \link sbn::basic_pipeline <em>queues</em>\endlink.
Each device has its own queue (there are queues for processors, disks and
network cards) and a thread pool that processes kernels from the queue. Queues
allow devices to work in parallel: process one part of the data via processor queue
and simultaneously write another one via disk queue.

Each programme begins with creating all the necessary kernel queues, starting
the corresponding threads and sending the main kernel to one of the queues.
After that programme execution resembles that of sequential programme with each
nested call to a procedure replaced with construction of a child kernel and
sending it to an appropriate queue.  The difference is that pipelines process
kernels \em asynchronously: the usual procedure code is decomposed into the
part that comes before nested procedure call (which goes into \link
sbn::kernel::act() <code>act()</code>\endlink method) and the part that comes
after the call (which goes to \link sbn::kernel::react
<code>react()</code>\endlink method).  In <code>act</code> method we create
child kernels instead of direct nested procedure calls, and in
<code>react()</code> method we collect the results of their execution.

\section api Hello world (single node)
\anchor introduction-api

The first step is to decide which kernel queues your programme needs. The
standard ones are
- \link sbn::parallel_pipeline CPU queue\endlink,
- \link sbnd::socket_pipeline NIC queue\endlink and
- \link sbn::parallel_pipeline timer queue\endlink (for periodic and
  schedule-based execution of kernels).

Standard queues for all devices except NIC are initialised automatically by
\link sbn::Factory \endlink class. All you have to do is to configure them and
start the threads.

\code{.cpp}
using namespace sbn;
factory_guard g;          // init queues and start threads
// send the main kernel to the queue (see below)
return wait_and_return(); // wait for the kernels to finish their work
\endcode

The second step is to subclass \link sbn::kernel <code>kernel</code>\endlink
and implement \c act() and \c react() methods for each sequential
stage of your programme and for parallel parts of each stage.

\code{.cpp}
struct Main: public sbn::kernel {
    void act() {
        std::cout << "Hello world" << std::endl;
        sbn::commit(std::move(this_ptr()));
    }
};
\endcode

\note Even if your programme does not have parallel parts, its performance may
still increase by constructing a separate kernel for each input
file. For example, to make a programme that searches files for matching pattern
(\c grep command in UNIX) parallel it is sufficient to construct a kernel for
each file and send all of them to CPU queue. A better way is to construct a
separate kernel to read portions of the files via I/O queue and for each
portion construct and send a new kernel to CPU queue to process it in
parallel.

Finally, you need to send the main kernel to the queue
via \link sbn::send <code>send</code>\endlink function.

\code{.cpp}
if (sbn::this_application::standalone()) {
    send(sbn::make_pointer<Main>(argc, argv));
}
\endcode

Use \link sbn::commit <code>commit</code>\endlink to return the kernel to its
parent and reclaim system resources.

\section failover Automatic failure handling
\anchor introduction-failover

In general, there are two types of failures occurring in any hierarchical
distributed system:

- failure of a \em child entity,
- failure of the \em parent entity.

\subsection kernel-failures Handling kernel failures
\anchor introduction-kernel-failures

Since any child kernel is part of a hierarchy the simplest method of
handling its failure is to let its parent restart it on a healthy cluster
node. Subordination does this automatically for any child kernel. This
approach works well unless your hierarchy is deep and require restarting a lot
of kernels upon a failure; however, this approach does not work for the main
kernel --- the first kernel of the programme that does not have a parent.

In case of the main kernel failure one solution is to keep a copy of it on
some other cluster node and restore from it when the former node fails.
Subordination implements this for any kernel with the \link
sbn::kernel_flag::carries_parent <code>carries_parent</code>\endlink flag set,
but the approach works only for those parent kernels that have only one
child at a time (extending algorithm to cover more cases is one of the
goals of ongoing research).

At present, a kernel is considered failed when a node to which it was sent
fails, and a node is considered failed when the corresponding connection closes
unexpectedly. There are no mechanisms that deal with unreliable connections
other than timeouts configured in operating system.

\subsection node-failures Handling cluster node failures
\anchor introduction-node-failures

Cluster node failures are much simpler to mitigate: there is no state to be
lost and the only invariant that should be preserved in a cluster is
connectivity of nodes. All nodes should "know" each other and be able to
establish arbitrary connections between each other; in other words, nodes
should be able to \em discover each other. Subordination implements this
functionality without distributed consensus algorithm: the framework builds tree
hierarchy of nodes using IP addresses and pre-set fan-out value to rank nodes.
Using this algorithm a node computes IP address of its would-be parent and
tries to connect to it; if the connection fails it tries another node from the
same or higher level of tree hierarchy. If it reaches the root of the tree and no
node responds, it becomes the root node. This algorithm is used both during node
bootstrap phase and upon a failure of any node.

\section hierarchy Hierarchical architecture
\anchor introduction-hierarchy

At high-level Subordination framework is composed of multiple layers:

- physical layer (fully-connected servers and network switches),
- middleware layer (hierarchy of nodes),
- application layer (hierarchy of kernels).

Load balancing is implemented by superimposing hierarchy of kernels on the
hierarchy of nodes: When node queues are overflown by kernels some of them
may be "spilled" to subordinate nodes (planned feature), much like water flows
from the top of a cocktail pyramid down to its bottom when volume of glasses
in the current layer is to small to hold it.

\section bottomup Bottom-up design
\anchor introduction-bottomup

Subordination framework uses [bottom-up](http://www.paulgraham.com/progbot.html)
source code development approach which means we create low-level abstractions
to simplify high-level code and make it clean and readable. There are three
layers of abstractions:

- operating system layer, which contains thin C++ wrappers
  for POSIX and Linux system calls and data structures, and
- generic C++ abstractions layer,
- top layer which contains failure handling logic.

We separate layers from each other via
<a href="https://erdani.com/publications/xp2000.pdf">policy-based programming</a>
and <a href="http://erdani.com/publications/traits.html">traits classes</a>.
