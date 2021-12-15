//
// Created by Chahak Tharani on 12/2/21.
//

#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include <iostream>
#include "gloo/allreduce_ring.h"
#include <gloo/allreduce.h>
#include <mpi.h>
#include <algorithm>
#include <numeric>

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

// Same prototype as MPI API.
int MPI_Barrier(MPI_Comm comm) {
    gloo::BarrierOptions opts(k_context);
    gloo::barrier(opts);
}

int MPI_Recv1(
        void *buf,
        ssize_t bytes,
        int source,
        int tag,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
    ubuf->recv(source, tag);
    ubuf->waitRecv();
}

int MPI_Send1(
        const void *cbuf,
        ssize_t bytes,
        int dest,
        int tag,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(const_cast<void*>(cbuf), bytes);
    ubuf->send(dest, tag);
    ubuf->waitSend();
}

int MPI_Recv(
        void *buf,
        ssize_t bytes,
        int source,
        int tag,
        int receiveOffset,
        size_t receiveBytes,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
    ubuf->recv(source, tag, receiveOffset, receiveBytes);
    ubuf->waitRecv();
}

int MPI_Send_n_Recieve(
        const void *ubuf,
        ssize_t sbytes,
        int dest,
        int dtag,
        const void *vbuf,
        ssize_t rbytes,
        int src,
        int stag,
        int sendOffset,
        int receiveOffset,
        size_t sendReceiveBytes,
        MPI_Comm comm) {
    auto sbuf = k_context->createUnboundBuffer(const_cast<void*>(ubuf), sbytes);
    auto rbuf = k_context->createUnboundBuffer(const_cast<void*>(vbuf), rbytes);
    sbuf->send(dest, dtag, sendOffset, sendReceiveBytes);
    rbuf->recv(src, stag, receiveOffset, sendReceiveBytes);
    sbuf->waitSend();
    rbuf->waitRecv();
}

int MPI_Send(
        const void *cbuf,
        ssize_t bytes,
        int dest,
        int tag,
        int sendOffset,
        size_t sendBytes,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(const_cast<void*>(cbuf), bytes);
    ubuf->send(dest, tag, sendOffset, sendBytes);
    ubuf->waitSend();
}

void runReduceScatter(int rank, int size, int inputEle) {
    int sendBuffer [inputEle];
    for (int j = 0; j < inputEle; j++){
        int num = rand() % 101;
        sendBuffer[inputEle] = num;
    }
    int recvBuffer[inputEle];
    int tag = 5643;
    int partner;
    int mask = size / 2;
    int begin = 0;
    int end = inputEle;
    int offset, sendOffset, receiveOffset;
    size_t sendReceiveBytes;

    while (mask){
        partner = mask ^ rank;
        std::fill(recvBuffer, recvBuffer + inputEle, 0); // TODO: (1)
        offset = begin + (end - begin) / 2;
        sendOffset = offset * sizeof(int);
        receiveOffset = begin * sizeof(int);
        sendReceiveBytes = (sendOffset - receiveOffset);
        MPI_Send_n_Recieve(
                sendBuffer, sizeof(sendBuffer), partner, tag,
                recvBuffer, sizeof(recvBuffer), partner, tag,
                sendOffset, receiveOffset, sendReceiveBytes,
                MPI_COMM_WORLD);
        for (int c = 0; c < inputEle /* TODO: (1) */; c++) {
            sendBuffer[c] += recvBuffer[c];
        }
        if (rank & mask){
            begin = offset;
        } else {
            end = offset;
        }
        mask >>= 1;
    }

    // run Gather
    if (rank == 0){
        for (int all = 1; all < size; all++) {
            int receiveOffset = all * inputEle / size * sizeof(int);
            int receiveBytes = inputEle / size * sizeof(int);
            MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag, receiveOffset, receiveBytes, MPI_COMM_WORLD);
            for (int c = all * inputEle/size; c < (all+1)*inputEle/size; c++) {
                sendBuffer[c] = recvBuffer[c];
            }
        }
    } else {
        int sendBytes = inputEle / size * sizeof(int);
        MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag, rank * inputEle / size * sizeof(int), sendBytes, MPI_COMM_WORLD);
    }
//    if (rank == 0) {
//        for (int c = 0; c < 8; c++) {
//            std::cout << sendBuffer[c] << " ";
//        }
//        std::cout << std::endl;
//    }
}

double runGather(const int rank , int size, double input) {
    double sendBuffer[] = {input};
    double recvBuffer[1];
    const int tag = 564;
    std::vector<double> allTimes;

    if (rank == 0){
        for (int all = 1; all < size; all++) {
//            std::cout << "Process waiting at " << rank << " for " << all << "\n";
            MPI_Recv1(recvBuffer, sizeof(recvBuffer), all, tag, MPI_COMM_WORLD);
            allTimes.push_back(recvBuffer[0]);
        }
    } else {
//        std::cout << "Sending from " << rank << " to root" << "\n";
        MPI_Send1(sendBuffer, sizeof(sendBuffer), 0, tag, MPI_COMM_WORLD);
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
//    std::cout << "Creating fileStore " << "\n";
    auto fileStore = gloo::rendezvous::FileStore("/proj/UWMadison744-F21/groups/akc/rendezvous_checkpoint-CT");
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

//    std::cout << "rank=" << rank << "size=" << size << std::endl;
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

//    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
//    std::cout << "Running bcast" << "\n";
    for (int i = 0; i < 10; i++) {
        runReduceScatter(rank, size, inputEle);
    }

    std::vector<double> all_stat;
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        const auto start = std::chrono::high_resolution_clock::now();
        runReduceScatter(rank, size, inputEle);
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
//    runReduceScatter(rank, size);
//    runGather(rank, size);
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
