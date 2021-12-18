/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 */

#include <iostream>
#include <memory>
#include <array>
#include <typeinfo>
#include <vector>
#include <numeric>
#include <mpi.h>
#include <gloo/barrier.h>
#include "gloo/reduce.h"
#include "gloo/transport/tcp/device.h"
#include "gloo/rendezvous/context.h"
#include "gloo/rendezvous/file_store.h"
#include "gloo/rendezvous/prefix_store.h"

// Usage:
//
// Open two terminals. Run the same program in both terminals, using
// a different RANK in each. For example:
//
// A: PREFIX=test1 SIZE=2 RANK=0 example_reduce
// B: PREFIX=test1 SIZE=2 RANK=1 example_reduce
//
// Expected output:
//
//   data[0] = 0
//   data[1] = 3
//   data[2] = 6
//   data[3] = 9
//

std::shared_ptr<gloo::Context> k_context;
int rank;
int size;
char hostname[HOST_NAME_MAX];

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

double runGather(const int rank , int size, double input) {
    double sendBuffer[1] = {input};
    double recvBuffer[1] = {0};
    const int tag = 564;
    std::vector<double> allTimes;

    if (rank != 0) {
        MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag, MPI_COMM_WORLD);
    } else {
        for (int all = 1; all < size; all++) {
            MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag, MPI_COMM_WORLD);
            allTimes.push_back(recvBuffer[0]);
        }
    }

    if (rank == 0) {
        auto it  = std::max_element(std::begin(allTimes), std::end(allTimes));
        return *it;
    } else{
        return 0;
    }
}

void mysum(void* c_, const void* a_, const void* b_, int n) {
    printf("n=%d\r\n", n);
    int* c = static_cast<int*>(c_);
    const int* a = static_cast<const int*>(a_);
    const int* b = static_cast<const int*>(b_);
    for (auto i = 0; i < n; i++) {
        printf("a[%d]=%d\r\n", i, a[i]);
        printf("b[%d]=%d\r\n", i, b[i]);
        c[i] = a[i] + b[i];
        printf("c[%d]=%d\r\n", i, c[i]);
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

int main(void) {
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

    // All connections are now established. We can now initialize some
    // test data, instantiate the collective algorithm, and run it.
    int *inputPointers = reinterpret_cast<int*>(malloc(sizeof(int) * inputEle));
    int *outputPointers = reinterpret_cast<int*>(malloc(sizeof(int) * inputEle));
    gloo::ReduceOptions opts(k_context);
    opts.setInput(inputPointers, inputEle);
    opts.setOutput(outputPointers, inputEle);
    for (int i = 0; i < inputEle; i++) {
        inputPointers[i] = i * (rank + 1);
        outputPointers[i] = 0;
    }

    void (*fn)(void*, const void*, const void*, int) = &mysum;
    opts.setReduceFunction(fn);

    // A small maximum segment size triggers code paths where we'll
    // have a number of segments larger than the lower bound of
    // twice the context size.
    opts.setMaxSegmentSize(128);
    opts.setRoot(size - 1);
//    reduce(opts);

    for (int i = 0; i < 10
    ; i++) {
        reduce(opts);
    }

    std::vector<double> all_stat;
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        const auto start = std::chrono::high_resolution_clock::now();
        reduce(opts);
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> ets = end - start;
        const double elapsed_ts = ets.count();
        MPI_Barrier(MPI_COMM_WORLD);
        double maxTime = runGather(rank, size, elapsed_ts);
        if (rank == 0) {
            std::cout << "max timing for " << i << " is " << maxTime << std::endl;
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
    }
    // Print the result.
//    std::cout << "Output: " << std::endl;
//    for (int i = 0; i < 4; i++) {
//        std::cout << "data = " << outputPointers[i] << std::endl;
//    }

    return 0;
}