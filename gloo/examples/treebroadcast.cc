#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include "gloo/allreduce_ring.h"
#include <gloo/allreduce.h>

#include <gloo/barrier.h>
#include "gloo/transport/tcp/device.h"
#include "gloo/rendezvous/context.h"
#include "gloo/rendezvous/file_store.h"
#include "gloo/rendezvous/prefix_store.h"


#include <unistd.h>
#include <limits.h>

std::shared_ptr<gloo::Context> k_context;
int rank;
int size;
char hostname[HOST_NAME_MAX];

int MPI_Recv(
        void *buf,
        ssize_t bytes,
        int source,
        int tag) {
    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
    ubuf->recv(source, tag);
    ubuf->waitRecv();
}

int MPI_Send(
        const void *cbuf,
        ssize_t bytes,
        int dest,
        int tag) {
    auto ubuf = k_context->createUnboundBuffer(const_cast<void*>(cbuf), bytes);
    ubuf->send(dest, tag);
    ubuf->waitSend();
}

int MPI_Barrier() {
    gloo::BarrierOptions opts(k_context);
    gloo::barrier(opts);
}

void run(int rank, int size) {

    if (rank == 0) {
        int dst = 1;
        int a[] = {1, 1, 2, 2, 3, 3, 4, 4};
        int tag = 1234;
        MPI_Send(a, sizeof(a), dst, tag);
        dst = 2;
        MPI_Send(a, sizeof(a), dst, tag);
//        for (int dst = 1; dst < size; dst++) {
//            int a[] = {dst, 1, 2, 2, 3, 3, 4, 4};
//            int tag = 1234;
//            MPI_Send(a, sizeof(a), dst, tag);
//        }
    } else {
        int src = 0;
        if (rank % 2 == 0) {
            src = (rank - 2) / 2;
        } else {
            src = (rank - 1) / 2;
        }
        int tag = 1234;
        int a[8];
        MPI_Recv(a, sizeof(a), src, tag);
	    gethostname(hostname, HOST_NAME_MAX);
        std::cout << "Received from rank " << src << " to " << rank << " on " << hostname << std::endl;
        std::cout << "\t";
        for (int i = 0; i < sizeof(a) / sizeof(*a); i++) {
            std::cout << a[i] << " ";
        }
        std::cout << std::endl;
        int dst1 = 2 * rank + 1;
        int dst2 = 2 * rank + 2;
        if (dst1 < size) {
            std::cout << "Sending from " << rank << " to " << dst1 << std::endl;
            MPI_Send(a, sizeof(a), dst1, tag);
        }
        if (dst2 < size) {
            std::cout << "Sending from " << rank << " to " << dst2 << std::endl;
            MPI_Send(a, sizeof(a), dst2, tag);
        }
    }

    MPI_Barrier();
}

void runBcast(int rank, int size, int vsize) {
    std::cout << "Bcast " << rank << " " << size << "\n";
    int buffer[vsize];
    int tag = 5643;

    int logn = 1  << ( __builtin_ctz(rank));
    if (rank == 0) logn = 1  << (__builtin_clz(size));
    logn >>= 1;

    if (rank != 0) {
        const int partner = rank ^ (1 << __builtin_ctz(rank));\
        if (debug)
            std::cout << "Waiting at " << rank << " for " << partner << "\n";
        MPI_Recv(buffer, sizeof(buffer), partner, tag);
        if (debug)
            std::cout << "\tReceived" << sizeof(buffer) << "\n";
    }

    while (logn > 0) {
        const int partner = rank | logn;
        if (partner > rank && partner < size) {
            if (debug)
                std::cout << "Sending from " << rank << " to " << partner << "\n";
            MPI_Send(buffer, sizeof(buffer), partner, tag);
            if (debug)
                std::cout << "\tSent" << "\n";
        }
        logn >>= 1;
    }

//    MPI_Barrier();
}

void run2(int rank) {
    int a[8];
    gethostname(hostname, HOST_NAME_MAX);
    std::cout << "running " << rank << " @ " << hostname << "\n";
    bzero(a, sizeof(a));
    std::cout << " assigned zeros " << std::endl;
    if (rank == 0) {
        std::cout << " sending from 0 " << std::endl;
        a[2] = 123;
        MPI_Send(a, sizeof(a), 1, 100);
        std::cout << " sent " << std::endl;
    } else {
        std::cout << " receiving at 1 " << std::endl;
        MPI_Recv(a, sizeof(a), 0, 100);
        std::cout << " received " << std::endl;
        std::cout << "\t";
        for (int i = 0; i < sizeof(a) / sizeof(*a); i++) {
            std::cout << a[i] << " ";
        }
        std::cout << std::endl;
    }
}

void init(int rank, int size, std::string prefix, std::string network) {
    gloo::transport::tcp::attr attr;
    attr.iface = network;
//    attr.iface = "lo";
    attr.ai_family = AF_UNSPEC;

//    std::cout << "Creating device " << "\n";
    auto dev = gloo::transport::tcp::CreateDevice(attr);
//    std::cout << "Creating fileStore " << "\n";
    auto fileStore = gloo::rendezvous::FileStore("/proj/UWMadison744-F21/groups/akc/rendezvous_checkpoint/");
//    std::cout << "Creating prefixStore " << "\n";
    auto prefixStore = gloo::rendezvous::PrefixStore(prefix, fileStore);
//    std::cout << "Creating context " << "\n";
    auto context = std::make_shared<gloo::rendezvous::Context>(rank, size);
//    std::cout << "Creating fullMesh " << "\n";
    context->connectFullMesh(prefixStore, dev);
//    std::cout << "Creating kContext " << "\n";
    k_context = std::move(context);
    rank = k_context->rank;
    size = k_context->size;

    std::cout << "rank=" << rank << "size=" << size << std::endl;
}

int main(int argc, char* argv[]) {
    if (getenv("PREFIX") == nullptr ||
        getenv("SIZE") == nullptr ||
        getenv("RANK") == nullptr ||
        getenv("NETWORK") == nullptr ||
        getenv("VSIZE") == nullptr) {
        if (getenv("VSIZE") == nullptr)
            std::cerr << "Please set VSIZE" << std::endl;
        std::cerr
                << "Please set environment variables PREFIX, SIZE, RANK NETWORK and VSIZE"
                << std::endl;
        return 1;
    }
    std::string prefix = getenv("PREFIX");
    int rank = atoi(getenv("RANK"));
    int size = atoi(getenv("SIZE"));
    int vsize = atoi(getenv("VSIZE"));
    std::string network = getenv("NETWORK");

//    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
//    std::cout << "Running bcast" << "\n";
    runBcast(rank, size, vsize);
    std::cout << "Done" << "\n";
    return 0;
}