#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include "gloo/allreduce_ring.h"
#include <gloo/allreduce.h>
#include <mpi.h>

#include <gloo/barrier.h>
#include "gloo/mpi/context.h"
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
        int tag,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
    ubuf->recv(source, tag);
    ubuf->waitRecv();
}

int MPI_Send(
        const void *cbuf,
        ssize_t bytes,
        int dest,
        int tag,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(const_cast<void*>(cbuf), bytes);
    ubuf->send(dest, tag);
    ubuf->waitSend();
}

void run() {

    if (rank == 0) {
        int dst = 1;
        int a[] = {1, 1, 2, 2, 3, 3, 4, 4};
        int tag = 1234;
        MPI_Send(a, sizeof(a), dst, tag, MPI_COMM_WORLD);
        dst = 2;
        MPI_Send(a, sizeof(a), dst, tag, MPI_COMM_WORLD);
//        for (int dst = 1; dst < size; dst++) {
//            int a[] = {dst, 1, 2, 2, 3, 3, 4, 4};
//            int tag = 1234;
//            MPI_Send(a, sizeof(a), dst, tag, MPI_COMM_WORLD);
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
        MPI_Recv(a, sizeof(a), src, tag, MPI_COMM_WORLD);
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
            MPI_Send(a, sizeof(a), dst1, tag, MPI_COMM_WORLD);
        }
        if (dst2 < size) {
            std::cout << "Sending from " << rank << " to " << dst2 << std::endl;
            MPI_Send(a, sizeof(a), dst2, tag, MPI_COMM_WORLD);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

void run2(int rank) {
    int a[8];
    gethostname(hostname, HOST_NAME_MAX);
    std::cout << "running " << rank << " @ " << hostname << "\n";
    bzero(a, sizeof(a));
    std::cout << " assigned zeros " << std::endl;
    if (rank == 0) {
        std::cout << " sending from 0 " << std::endl;
        MPI_Send(a, sizeof(a), 1, 100, MPI_COMM_WORLD);
        std::cout << " sent " << std::endl;
    } else {
        std::cout << " receiving at 1 " << std::endl;
        MPI_Recv(a, sizeof(a), 0, 100, MPI_COMM_WORLD);
        std::cout << " received " << std::endl;
        std::cout << "\t";
        for (int i = 0; i < sizeof(a) / sizeof(*a); i++) {
            std::cout << a[i] << " ";
        }
        std::cout << std::endl;
    }
}

void init(int rank, int size, std::string prefix) {
    gloo::transport::tcp::attr attr;
    attr.iface = "enp6s0f0";
//    attr.iface = "lo";
    attr.ai_family = AF_UNSPEC;

    auto dev = gloo::transport::tcp::CreateDevice(attr);
    auto fileStore = gloo::rendezvous::FileStore("/proj/uwmadison744-f21-PG0/rendezvous_checkpoint");
    auto prefixStore = gloo::rendezvous::PrefixStore(prefix, fileStore);
    auto context = std::make_shared<gloo::rendezvous::Context>(rank, size);
    context->connectFullMesh(prefixStore, dev);
    k_context = std::move(context);
    rank = k_context->rank;
    size = k_context->size;

    std::cout << "rank=" << rank << "size=" << size << std::endl;
}

int main(int argc, char* argv[]) {
    if (getenv("PREFIX") == nullptr ||
        getenv("SIZE") == nullptr ||
        getenv("RANK") == nullptr) {
        std::cerr
                << "Please set environment variables PREFIX, SIZE, and RANK."
                << std::endl;
        return 1;
    }
    std::string prefix = getenv("PREFIX");
    int rank = atoi(getenv("RANK"));
    int size = atoi(getenv("SIZE"));

    init(rank, size, prefix);
    run2(rank);
    return 0;
}