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

auto MPI_ISend(
        const void *cbuf,
        ssize_t bytes,
        int dest,
        int tag) {
    auto ubuf = k_context->createUnboundBuffer(const_cast<void*>(cbuf), bytes);
    ubuf->send(dest, tag);
    return std::move(ubuf);
}

int MPI_SendRecv(
        const void *sendbuf,
        const void *recvbuf,
        ssize_t send_bytes,
        ssize_t recv_bytes,
        int dest,
        int src,
        int tag)
{
    auto usendbuf = k_context->createUnboundBuffer(const_cast<void*>(sendbuf), send_bytes);
    auto urecvbuf = k_context->createUnboundBuffer(const_cast<void*>(recvbuf), recv_bytes);
    usendbuf->send(dest, tag);
    urecvbuf->recv(src, tag);
    usendbuf->waitSend();
    urecvbuf->waitRecv();
}

int MPI_Barrier() {
    gloo::BarrierOptions opts(k_context);
    gloo::barrier(opts);
}

void runBcast(int rank, int size, int arrSize) {
    int debug = 0;
    if (debug)
        std::cout << "Bcast " << rank << " " << size << "\n";
    int tag = 5643;
    int val;

    if (debug)
        std::cout << "Running scatter on " << rank << "\n";
    int recvbuf[arrSize];
    int sendbuf[arrSize];

    int w;
    int n = size;
    int count = arrSize / 4;

    if (rank == 0) {
        if (__builtin_popcount(n) > 1) // count number of 1s set in the binary representation
            w = (1 << (32-__builtin_clz(n)));
        else w = n;
    } else
        w = (1 << __builtin_ctz(rank)); // count trailing number of 0s in binary representation
    if (rank != 0) {
        if (debug)
            std::cout << "\tWaiting to receive from " << (rank ^ w) << "\n";
        MPI_Recv(recvbuf + (rank * arrSize / size), sizeof(int) * (count * w), rank ^ w, tag);

        if (debug) {
            std::cout << "\tReceived ";
            for (int i = 0; i < count * w; i++) {
                std::cout << recvbuf[i + (rank * arrSize / size)] << " ";
            }
            std::cout << "\n";
        }
    } else {
    }

    int k = 0;
    const int cn = count * n;
    static std::vector<std::unique_ptr<gloo::transport::UnboundBuffer>> pending_req;
    pending_req.clear();
    while (w > 0) {
        const int partner = rank | w;
        if (debug)
            std::cout << "w=" << w << "\n";
        if (partner > rank && partner < n) {
            const int wc = w * count;
            const int bytes = ((wc << 1) >= cn) ? (cn - wc): wc;
            if (debug) {
                for (int i = 0; i < bytes; i++) {
                    std::cout << "\tSending new sendbuf[" << ((partner * arrSize) / size + i) << "]"
                              << sendbuf[(partner * arrSize) / size + i] << " to " << partner << " w=" << w
                              << " count=" << count << " bytes=" << bytes << "\n";
                }
            }
            pending_req.push_back(std::move(MPI_ISend(sendbuf + partner * arrSize / size, bytes * sizeof(int), partner, tag)));
        }
        w >>= 1;
    }

    for (auto& i: pending_req) i->waitSend();

    if (debug) {
        std::cout << "Received Array: ";
        for (int i = 0; i < cn; i++) {
            std::cout << recvbuf[i] << " ";
        }
        std::cout << "\n";
        std::cout << "Running gather on " << rank << "\n";
    }

    // Ring All gather
    const int partner = (rank + 1) % n;
    const int partnerp = (rank - 1 + n) % n;
    int ri = rank, rp = rank - 1;
    if (rp < 0) rp = n - 1;
    for (int i = 0; i < n - 1; ++i) {

        if (rank == 0) {
            if (debug) {
                for (int j = 0; j < count; j++) {
                    std::cout << "\tSending buffer[" << (ri * count + j) << "] = " << sendbuf[(ri * count + j)]
                              << " from " << rank << " to " << partner << " and receiving from "
                              << partnerp << " in buffer[" << (rp * count + j) << "]\n";
                }
            }
            MPI_SendRecv(sendbuf + ri * count, sendbuf + rp * count,
                         sizeof(sendbuf[ri * count]) * count, sizeof(sendbuf[rp * count]) * count,
                         partner, partnerp, tag);
            if (debug) {
                for (int j = 0; j < count; j++) {
                    std::cout << "\tSent=" << sendbuf[ri * count + j] << " Received=" << sendbuf[rp * count + j]
                              << "\n";
                }
            }
        } else {
            if (debug) {
                for (int j = 0; j < count; j++) {
                    std::cout << "\tSending buffer[" << (ri * count + j) << "] = " << recvbuf[(ri * count + j)]
                              << " from " << rank << " to " << partner << " and receiving from "
                              << partnerp << " in buffer[" << (rp * count + j) << "]\n";
                }
            }
            MPI_SendRecv(recvbuf + ri * count, recvbuf + rp * count,
                         sizeof(recvbuf[ri * count]) * count, sizeof(recvbuf[rp * count]) * count,
                         partner, partnerp, tag);
            if (debug) {
                for (int j = 0; j < count; j++) {
                    std::cout << "\tSent=" << recvbuf[ri * count + j] << " Received=" << recvbuf[rp * count + j]
                              << "\n";
                }
            }
        }


        if (--ri == -1) ri = n-1;
        if (--rp == -1) rp = n-1;
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

//    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
//    std::cout << "Running bcast" << "\n";

    for (int i = 0; i < 10; i++) {
        runBcast(rank, size, vsize);
    }

    std::vector<double> all_stat;
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier();
        const auto start = std::chrono::high_resolution_clock::now();
        runBcast(rank, size, vsize);
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
//        double sum = std::accumulate(all_stat.begin(), all_stat.end(), 0.0);
//        std::sort(all_stat.begin(), all_stat.end());
//        double median = all_stat[all_stat.size()/2];
//        double avg = sum / iterations;
//        std::cout << "median is " << median << "\n";
//        std::cout << "average is " << avg << std::endl;
        std::cout << "stats: ";
        for (auto ts : all_stat) std::cout << ts << " ";
        std::cout << "\n";
    }


    std::cout << "Done" << "\n";
    return 0;
}