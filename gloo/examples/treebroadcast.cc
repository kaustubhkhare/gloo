#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <numeric>
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

void runBcast(int rank, int size, int vsize, int* buffer) {
    int debug = 0;
    if (debug)
        std::cout << "Bcast " << rank << " " << size << "\n";
    int tag = 5643;

    int logn = 1  << ( __builtin_ctz(rank));
    if (rank == 0) logn = 1  << (__builtin_clz(size));
    logn >>= 1;

    if (rank != 0) {
        const int partner = rank ^ (1 << __builtin_ctz(rank));\
        if (debug)
            std::cout << "Waiting at " << rank << " for " << partner << "\n";
        MPI_Recv(buffer, sizeof(buffer) * vsize, partner, tag);
        if (debug)
            std::cout << "\tReceived" << sizeof(buffer) << "\n";
    }

    while (logn > 0) {
        const int partner = rank | logn;
        if (partner > rank && partner < size) {
            if (debug)
                std::cout << "Sending from " << rank << " to " << partner << "\n";
            MPI_Send(buffer, sizeof(buffer) * vsize, partner, tag);
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

double runGather(const int rank , int size, double input) {
    double sendBuffer[] = {input};
    double recvBuffer[1];
    const int tag = 564;
    std::vector<double> allTimes;

    if (rank == 0){
        for (int all = 1; all < size; all++) {
//            std::cout << "Process waiting at " << rank << " for " << all << "\n";
            MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag);
            allTimes.push_back(recvBuffer[0]);
        }
    } else {
//        std::cout << "Sending from " << rank << " to root" << "\n";
        MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag);
//        std::cout << "\tSent" << "\n";
    }

    if (rank == 0) {
        auto it  = std::max_element(std::begin(allTimes), std::end(allTimes));
        return *it;
    } else{
        return 0;
    }
}

int main(int argc, char* argv[]) {
    if (getenv("PREFIX") == nullptr ||
        getenv("SIZE") == nullptr ||
        getenv("RANK") == nullptr ||
            getenv("ITERS") == nullptr ||
            getenv("NETWORK") == nullptr ||
            getenv("INPUT_SIZE") == nullptr) {
        std::cerr
                << "Please set environment variables PREFIX, SIZE, RANK NETWORK and VSIZE"
                << std::endl;
        return 1;
    }
    std::string prefix = getenv("PREFIX");
    int rank = atoi(getenv("RANK"));
    int size = atoi(getenv("SIZE"));
    int vsize = atoi(getenv("INPUT_SIZE"));
    int iterations = atoi(getenv("ITERS"));
    std::string network = getenv("NETWORK");

    int* buffer = new int[vsize];

//    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
//    std::cout << "Running bcast" << "\n";

    for (int i = 0; i < 10; i++) {
        runBcast(rank, size, vsize, buffer);
    }

    std::vector<double> all_stat;
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier();
        const auto start = std::chrono::high_resolution_clock::now();
        runBcast(rank, size, vsize, buffer);
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> ets = end - start;
        const double elapsed_ts = ets.count();
        MPI_Barrier();
        double maxTime = runGather(rank, size, elapsed_ts);
        if (rank == 0) {
//            std::cout << "max timing for " << i << " is " << maxTime << std::endl;
            all_stat.push_back(maxTime);
        }
    }

    if (rank == 0) {
        double sum = std::accumulate(all_stat.begin(), all_stat.end(), 0.0);
        std::sort(all_stat.begin(), all_stat.end());
        double median = all_stat[all_stat.size()/2];
        double avg = sum / iterations;
        std::cout << median << "\n";
        std::cout << avg << std::endl;
//        std::cout << "stats: ";
//        for (auto ts: all_stat) std::cout << ts << " ";
//        std::cout << "\n";
    }

    std::cout << "Done" << "\n";
    return 0;
}