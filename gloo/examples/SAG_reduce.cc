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

//int MPI_Recv(
//        void *buf,
//        ssize_t bytes,
//        int source,
//        int tag,
//        int recvOffset,
//        size_t receiveBytes,
//        MPI_Comm comm) {
//    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
//    ubuf->recv(source, tag, recvOffset, receiveBytes);
//    ubuf->waitRecv();
//}

int MPI_Send_n_Recieve(
        const void *ubuf,
        ssize_t sbytes,
        int dest,
        int dtag,
        const void *vbuf,
        ssize_t rbytes,
        int src,
        int stag,
        MPI_Comm comm) {
    auto sbuf = k_context->createUnboundBuffer(const_cast<void*>(ubuf), sbytes);
    auto rbuf = k_context->createUnboundBuffer(const_cast<void*>(vbuf), rbytes);
    sbuf->send(dest, dtag);
    rbuf->recv(src, stag);
    sbuf->waitSend();
    rbuf->waitRecv();
}

int GT_Gather(void *sendbuf_,
              void *recvbuf_, int input_size,
              int rank, int size);

void runReduceScatter(int rank, int size, int input_size, int* sendBuffer, int* recvBuffer) {
    const int tag = 5643;
    int partner;
    int mask = size / 2;
    int begin = 0;
    int end = input_size;
    int sendOffset, recvOffset;

//    std::fill(recvBuffer, recvBuffer + input_size, 0);
    int round = 0;
    while (mask) {
        partner = mask ^ rank;
        const int mid = (end + begin) / 2;
        int sendBytes;
        int recvBytes;
        int endOffset;
        sendBytes = (mid - begin) * sizeof(int);
        recvBytes = (end - mid) * sizeof(int);
        sendOffset = begin;
        recvOffset = mid;
        endOffset = end;
        if (!(rank & mask)) {
            std::swap(sendOffset, recvOffset);
            std::swap(sendBytes, recvBytes);
            endOffset = mid;
        }
        MPI_Send_n_Recieve(
                sendBuffer + sendOffset, sendBytes, partner, tag,
                recvBuffer + recvOffset, recvBytes, partner, tag,
                MPI_COMM_WORLD);
        std::cout << "[" << rank << " / " << round << "] S [ "
                << sendOffset << ":" << sendBytes/sizeof(int) << "] R ["
                << recvOffset << ":" << recvBytes/sizeof(int) << "] - ";
        for (int c = recvOffset; c < endOffset; c++) {
            sendBuffer[c] += recvBuffer[c];
            std::cout << sendBuffer[c] << ", ";
        }
        std::cout << std::endl;
        
        if (rank & mask){
            begin = mid;
        } else {
            end = mid;
        }
        round++;
        mask >>= 1;
    }

    std::cout << "[" << rank << "] " << "At the end of scatter:";
    for (int i = 0; i < input_size; i++)
        std::cout << sendBuffer[i] << ", ";
    std::cout << std::endl;

    // run Gather
    GT_Gather(sendBuffer, sendBuffer, input_size, rank, size);
    std::cout << "\n\n";
    std::cout << "[" << rank << "] " << "At the end of gather:";
    for (int i = 0; i < input_size; i++)
        std::cout << sendBuffer[i] << ", ";


    //    if (rank == 0){
//        for (int all = 1; all < size; all++) {
//            int recvOffset = all * input_size / size * sizeof(int);
//            int receiveBytes = input_size / size * sizeof(int);
//            MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag, recvOffset, receiveBytes, MPI_COMM_WORLD);
//            for (int c = all * input_size/size; c < (all+1)*input_size/size; c++) {
//                sendBuffer[c] = recvBuffer[c];
//            }
//        }
//    } else {
//        int sendBytes = input_size / size * sizeof(int);
//        MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag, rank * input_size / size * sizeof(int), sendBytes, MPI_COMM_WORLD);
//    }
//    if (rank == 0) {
//        for (int c = 0; c < 8; c++) {
//            std::cout << sendBuffer[c] << " ";
//        }
//        std::cout << std::endl;
//    }
}

int GT_Gather(void *sendbuf_,
              void *recvbuf_, int input_size,
              int rank, int size) {
    const int tag = 5643;
    int* sendbuf = (int*)sendbuf_;
    int* recvbuf = (int*)recvbuf_;
    const int count = input_size / size;

    if ( (rank & 1) == 1) {
        std::cout << "Send " << rank << " to " << (rank ^ 1) << " val=";
        for (int i = 0; i < count; i++) {
            std::cout << (rank * count + i) << ":" << *(sendbuf + rank * count + i) << " ";
        }
        std::cout << "\n";
        return MPI_Send(sendbuf + rank * count, count * sizeof(int), rank ^ 1, tag, MPI_COMM_WORLD);
    }
//        return MPI_Send(sendbuf + rank * count, count, recvtype, rank ^ 1, 1, comm);

//    if (rank != 0) {
//        const int full_sz_b = input_size * sizeof(int);
//        if (full_sz_b > glob_sz) {
//            glob_buf = (int*)realloc(glob_buf, full_sz * sizeof(int));
//            glob_sz = full_sz * sizeof(int);
//        }
//        recvbuf = glob_buf;
//    }

    int bitm = 1;
//    if ((rank & 1) == 0)
//        memcpy(recvbuf + rank * count, sendbuf, count * sizeof(int));
    int szz = count;
    while (bitm < size) {
        const int partner = rank ^ bitm;
        const int offset = rank * count + bitm * count;
        std::cout << "bitm " << bitm << "\n";
        if (rank & bitm) {
//                printf("[%d] %d --> %d, off = %d, sz = %d\n", bitm, rank, partner, offset - szz, szz);
            std::cout << "Send " << rank << " to " << (partner) << " val=";
            for (int i = 0; i < szz; i++) {
                std::cout << (offset - bitm * count + i) << ":" << *(recvbuf + offset - bitm * count + i) << " ";
            }
            std::cout << "\n";
            return MPI_Send(recvbuf + offset - bitm * count, szz * sizeof(int), partner, tag, MPI_COMM_WORLD);
        } else if (partner < size) {
//            printf("[%d] %d <-- %d, off = %d, sz = %d\n", bitm, rank, partner, offset, szz);
            std::cout << "Receiving " << rank << " from " << (partner) << " at " << offset << "->" << offset + szz;
            std::cout << "\n";
            MPI_Recv(recvbuf + offset, szz * sizeof(int), partner, tag, MPI_COMM_WORLD);
            szz <<= 1;
        }
        bitm <<= 1;
    }
}



//
double runGather(const int rank , int size, double input) {
    double sendBuffer[] = {input};
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
    int input_size = atoi(getenv("INPUT_SIZE"));

//    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
//    std::cout << "Running bcast" << "\n";
//    for (int i = 0; i < 10; i++) { TODO
//        runReduceScatter(rank, size, input_size);
//    }

    std::vector<double> all_stat;
    constexpr int N = 8;
    int send_buf[N], recv_buf[N];
    std::fill(recv_buf, recv_buf + N, 0);
    for (int i = 0; i < N; i++) {
        send_buf[i] = i + rank * 100;
    }
    for (int i = 0; i < iterations; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        const auto start = std::chrono::high_resolution_clock::now();
        input_size = N;
        runReduceScatter(rank, size, input_size, send_buf, recv_buf);
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
