//
// Created by Chahak Tharani on 12/2/21.
//

#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>
#include <numeric>
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
#include <math.h>
#include <limits.h>

std::shared_ptr<gloo::Context> k_context;
int rank;
int size;
char hostname[HOST_NAME_MAX];

// Same prototype as MPI API.
int MPI_Barrier(MPI_Comm comm) {
    gloo::BarrierOptions opts(k_context);
    gloo::barrier(opts);
}

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

void runTreeReduce(const int rank, int size, int inputEle, int* sendBuffer, int* recvBuffer) {
    const int tag = 5643;
    int partner;
    int round = log2(size);
    int mask = 1;
//    std::cout << "tree_red run: " << round << " " << sizeof(sendBuffer) << " " << inputEle << "\n";
    while (round) {
        partner = rank ^ mask;

        if (rank & mask ) {
            MPI_Send(sendBuffer, sizeof(int) * inputEle, partner, tag, MPI_COMM_WORLD);
            return;
        } else {
            MPI_Recv(recvBuffer, sizeof(int) * inputEle, partner, tag, MPI_COMM_WORLD);
            for (int c = 0; c < inputEle; c++) {
                sendBuffer[c] = sendBuffer[c] + recvBuffer[c];
            }
        }

        mask <<= 1;
        round--;
    }
}

double runGather(const int rank , int size, double input) {
    double sendBuffer[1] = {input};
    double recvBuffer[1];
    const int tag = 564;
    std::vector<double> allTimes;

    if (rank == 0){
        for (int all = 1; all < size; all++) {
//            std::cout << "Process waiting at " << rank << " for " << all << "\n";
            MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag, MPI_COMM_WORLD);
            allTimes.push_back(recvBuffer[0]);
        }
    } else {
//        std::cout << "Sending from " << rank << " to root" << "\n";
        MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag, MPI_COMM_WORLD);
//        std::cout << "\tSent" << "\n";
    }

    if (rank == 0) {
        auto it  = std::max_element(std::begin(allTimes), std::end(allTimes));
        return *it;
    } else{
        return 0;
    }
}
void init(int rank, int size, std::string prefix, std::string network) {
    gloo::transport::tcp::attr attr;
    attr.iface = network;
    attr.ai_family = AF_UNSPEC;

    auto dev = gloo::transport::tcp::CreateDevice(attr);
    auto fileStore = gloo::rendezvous::FileStore("/proj/UWMadison744-F21/groups/akc/rendezvous_checkpoint-CT");
    auto prefixStore = gloo::rendezvous::PrefixStore(prefix, fileStore);
    auto context = std::make_shared<gloo::rendezvous::Context>(rank, size);
    context->connectFullMesh(prefixStore, dev);
    k_context = std::move(context);
    rank = k_context->rank;
    size = k_context->size;
}

int main(int argc, char* argv[]) {
    if (getenv("PREFIX") == nullptr ||
        getenv("SIZE") == nullptr ||
        getenv("RANK") == nullptr ||
            getenv("ITERS") == nullptr ||
            getenv("NETWORK") == nullptr ||
            getenv("INPUT_SIZE") == nullptr) {
        std::cerr
                << "Please set environment variables PREFIX, SIZE, and RANK."
                << std::endl;
        return 1;
    }
    std::string prefix = getenv("PREFIX");
    int rank = atoi(getenv("RANK"));
    int size = atoi(getenv("SIZE"));
    int iterations = atoi(getenv("ITERS"));
    std::string network = getenv("NETWORK");
    int inputEle = atoi(getenv("INPUT_SIZE"));

    init(rank, size, prefix, network);
    int sendBuffer[inputEle];
    int recvBuffer[inputEle] = {0};

    for (int i = 0; i < 10; i++) {
        runTreeReduce(rank, size, inputEle, sendBuffer, recvBuffer);
    }

    std::vector<double> all_stat;
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        const auto start = std::chrono::high_resolution_clock::now();
        runTreeReduce(rank, size, inputEle, sendBuffer, recvBuffer);
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> ets = end - start;
        const double elapsed_ts = ets.count();
        MPI_Barrier(MPI_COMM_WORLD);
        double maxTime = runGather(rank, size, elapsed_ts);
        if (rank == 0) {
//            std::cout << "max timing for " << i << " is " << maxTime << std::endl;
            all_stat.push_back(maxTime);
        }
    }
//
//    double sum = std::accumulate(all_stat.begin(), all_stat.end(), 0.0);
//    std::sort(all_stat.begin(), all_stat.end());
//    double median = all_stat[all_stat.size()/2];
//    double avg = sum / iterations;
//    std::cout << "median is " << median << "\n";
//    std::cout << "average is " << avg << std::endl;

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
    return 0;
}