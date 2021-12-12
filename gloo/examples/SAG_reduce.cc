//
// Created by Chahak Tharani on 12/2/21.
//

#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include "gloo/allreduce_ring.h"
#include <gloo/allreduce.h>
#include <mpi.h>
#include <algorithm>

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
        int receiveOffset,
        size_t receiveBytes,
        MPI_Comm comm) {
    auto ubuf = k_context->createUnboundBuffer(buf, bytes);
    ubuf->recv(source, tag, receiveOffset, receiveBytes);
    ubuf->waitRecv();
}

// Same prototype as MPI API.
int MPI_Barrier(MPI_Comm comm) {
//    ASSERT(comm == MPI_COMM_WORLD);
    gloo::BarrierOptions opts(k_context);
    gloo::barrier(opts);
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

void runReduceScatter(int rank, int size) {
    std::cout << "Bcast " << rank << " " << size << "\n";
    int sendBuffer [] = {1, 2, 3, 4, 5, 6, 7, 8};
    int recvBuffer[8] = {0};
    int tag = 5643;
    int partner;
    int mask = size / 2;
    int begin = 0;
    int end = 8;
    int offset, sendOffset, receiveOffset;
    size_t sendReceiveBytes;

    while (mask){
        partner = mask ^ rank;

        if (rank & mask){
            std::fill(recvBuffer, recvBuffer + 8, 0);
            offset = begin + (end - begin) / 2;
            sendOffset = begin * sizeof(int);
            std::cout << "Send offset" << sendOffset << "\n";
            receiveOffset = offset * sizeof(int);
            std::cout << "Send offset" << receiveOffset << "\n";
            sendReceiveBytes = (receiveOffset - sendOffset);
            std::cout << "Send offset" << sendReceiveBytes << "\n";

            std::cout << "Sending from " << rank << " to " << partner << "\n";
            MPI_Send_n_Recieve(
                    sendBuffer, sizeof(sendBuffer), partner, tag,
                    recvBuffer, sizeof(recvBuffer), partner, tag,
                    sendOffset, receiveOffset, sendReceiveBytes,
                    MPI_COMM_WORLD);
            std::cout << "Received buffer" << "\n";
            for (int c = 0; c < 8 ; c++) {
                std::cout << recvBuffer[c] << " ";
            }
            std::cout << "Process waiting at " << rank << " for " << partner << "\n";
            std::cout << "\tSent and Received" << "\n";
            std::cout << "Reducing the output on rank " <<rank<< std::endl;
            for (int c = 0; c < 8 ; c++) {
                sendBuffer[c] = sendBuffer[c] + recvBuffer[c];
                std::cout << sendBuffer[c] << " ";
            }
            std::cout << std::endl;
            begin = offset;
        } else {
            std::fill(recvBuffer, recvBuffer + 8, 0);
            offset = begin + (end - begin) / 2;
            sendOffset = offset * sizeof(int);
            std::cout << "Send offset" << sendOffset << "\n";
            receiveOffset = begin * sizeof(int);
            std::cout << "Send offset" << receiveOffset << "\n";
            sendReceiveBytes = (sendOffset - receiveOffset);
            std::cout << "Send offset" << sendReceiveBytes << "\n";
            std::cout << "Sending from " << rank << " to " << partner << "\n";
            MPI_Send_n_Recieve(
                    sendBuffer, sizeof(sendBuffer), partner, tag,
                    recvBuffer, sizeof(recvBuffer), partner, tag,
                    sendOffset, receiveOffset, sendReceiveBytes,
                    MPI_COMM_WORLD);
            std::cout << "Received buffer" << "\n";
            for (int c = 0; c < 8 ; c++) {
                std::cout << recvBuffer[c] << " ";
            }
            std::cout << "\tSent" << "\n";
            std::cout << "Process waiting at " << rank << " for " << partner << "\n";
            std::cout << "\tReceived" << "\n";
            std::cout << "Reducing the output on rank " <<rank<< std::endl;
            for (int c = 0; c < 8; c++) {
                sendBuffer[c] = sendBuffer[c] + recvBuffer[c];
                std::cout << sendBuffer[c] << " ";
            }
            std::cout << std::endl;
            end = offset;
        }
        mask >>= 1;
    }

    // run Gather
        if (rank == 0){
            for (int all = 1; all < size; all++) {
                std::cout << "Process waiting at " << rank << " for " << partner << "\n";
                int receiveOffset = all * 8 / size * sizeof(int);
                int receiveBytes = 8 / size * sizeof(int);
                MPI_Recv(recvBuffer, sizeof(recvBuffer), all, tag, receiveOffset, receiveBytes, MPI_COMM_WORLD);
                std::cout << "\tReceived" << "\n";
                for (int c = all * 8/size; c < (all+1)*8/size; c++) {
                    sendBuffer[c] = recvBuffer[c];
                    std::cout << sendBuffer[c] << " ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "Sending from " << rank << " to root" << "\n";
            int sendBytes = 8 / size * sizeof(int);
            MPI_Send(sendBuffer, sizeof(sendBuffer), 0, tag, rank * 8 / size * sizeof(int), sendBytes, MPI_COMM_WORLD);
            std::cout << "\tSent" << "\n";
        }
    if (rank == 0) {
        for (int c = 0; c < 8; c++) {
            std::cout << sendBuffer[c] << " ";
        }
        std::cout << std::endl;
    }
}


void init(int rank, int size, std::string prefix) {
    gloo::transport::tcp::attr attr;
    attr.iface = "enp9s4f0";
//    attr.iface = "lo";
    attr.ai_family = AF_UNSPEC;

//    std::cout << "Creating device " << "\n";
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

    std::cout << "Running init" << "\n";
    init(rank, size, prefix);
    std::cout << "Running bcast" << "\n";
    runReduceScatter(rank, size);
//    runGather(rank, size);
    return 0;
}
