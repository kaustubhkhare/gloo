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

void runBcast(int rank, int size) {
    std::cout << "Bcast " << rank << " " << size << "\n";
    int buffer[] = {444, 111, 222, 333};;
    int tag = 5643;
    int val;

    // Scatter
//    if (rank == 0) {
//        for (int i = 1; i < size; i++) {
//            std::cout << "Sending " << buffer[i] << " from 0 to " << i << "\n";
//            val = buffer[i];
//            MPI_Send(&val, sizeof(val), i, tag);
//            std::cout << "\tSend" << "\n";
//        }
//    } else {
//        std::cout << "Process waiting at " << rank << " for 0" << "\n";
//        MPI_Recv(&val, sizeof(val), 0, tag);
//        std::cout << "\tReceived " << val << " at " << rank << "\n";
//        buffer[rank] = val;
//    }


    int recvbuf[] = {0, 0, 0, 0};
    int sendbuf[] = {buffer[0], buffer[1], buffer[2], buffer[3]};
    int w;
    int n = size;
    int count = size;

    if (rank == 0) {
        if (__builtin_popcount(n) > 1)
            w = (1 << (32-__builtin_clz(n)));
        else w = n;
    } else
        w = (1 << __builtin_ctz(rank));
    if (rank != 0) {
//        if (rank & 1) sendbuf = recvbuf; // directly copy to leaf nodes
//        else if (n*count > glob_buf_sz) {
//            glob_buf = sendbuf = realloc(sendbuf, n*count*sizeof(int));
//            glob_buf_sz = n*count;
//        }
//        else sendbuf = glob_buf;
        MPI_Recv(recvbuf, sizeof(int) * (count * w), rank ^ w, tag);
//        MPI_Recv(sendbuf, count * w, MPI_INT, rank ^ w, 1, comm, MPI_STATUS_IGNORE);
    } else {
    }

    int k = 0;
    const int cn = count * n;
    static std::vector<std::unique_ptr<gloo::transport::UnboundBuffer>> pending_req;
    pending_req.clear();
    while (w > 0) {
        const int partner = rank | w;
        if (partner > rank && partner < n) {
            const int wc = w * count;
            const int bytes = ((wc << 1) >= cn) ? (cn - wc): wc;
            pending_req.push_back(std::move(MPI_ISend(sendbuf + w * count, bytes * sizeof(int), partner, tag)));
        }
        w >>= 1;
    }

    for (auto& i: pending_req) i->waitSend();






    // Ring All gather
    n = size;
    const int partner = (rank + 1) % n;
    const int partnerp = (rank - 1 + n) % n;
    int ri = rank, rp = rank - 1;
    count = size / size;
    if (rp < 0) rp = n - 1;
    for (int i = 0; i < n - 1; ++i) {
        std::cout << "Sending buffer[" <<  ri * count << "] = " << buffer[ri * count]
        << " from " << rank << " to " << partner << " and receiving from "
        << partnerp << " in buffer[" << rp * count << "]\n";
        MPI_SendRecv(buffer + ri * count, buffer + rp * count,
                     sizeof(buffer[ri * count]) * count, sizeof(buffer[rp * count] * count),
                     partner, partnerp, tag);
        std::cout << "\tSent=" + buffer[ri * count] << " Received=" << buffer[rp * count]
        << "\n";
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

int main(int argc, char* argv[]) {
    if (getenv("PREFIX") == nullptr ||
        getenv("SIZE") == nullptr ||
        getenv("RANK") == nullptr ||
        getenv("NETWORK") == nullptr) {
        std::cerr
                << "Please set environment variables PREFIX, SIZE, RANK and NETWORK"
                << std::endl;
        return 1;
    }
    std::string prefix = getenv("PREFIX");
    int rank = atoi(getenv("RANK"));
    int size = atoi(getenv("SIZE"));
    std::string network = getenv("NETWORK");

    std::cout << "Running init" << "\n";
    init(rank, size, prefix, network);
    std::cout << "Running bcast" << "\n";
    runBcast(rank, size);
    return 0;
}