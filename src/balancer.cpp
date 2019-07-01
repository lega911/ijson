
#include <unistd.h>
#include "balancer.h"
#include "server.h"
#include "connect.h"


void Balancer::start() {
    _thread = std::thread(&Balancer::_start, this);
}

void Balancer::_start() {
    int i, total, count_workers;
    int threads = server->threads;
    int *counts = (int*)_malloc(threads * sizeof(int));

    while(true) {
        usleep(500000);  // 500ms

        total = 0;
        count_workers = 0;
        memset(counts, 0, threads * sizeof(int));

        {
            auto lock = server->autolock();
            for(i=0;i<=server->max_fd;i++) {
                Connect *conn = server->connections[i];
                if(!conn) continue;
                u64 counter = conn->counter;
                int diff = (int)(counter - conn->counter_ext);
                if(!diff) continue;
                conn->counter_ext = counter;
                counts[conn->nloop] += diff;
                total += diff;
                count_workers++;
            }
        }

        int hi = 0;
        int low = 0;

        if(server->log & 64) std::cout << "balancer: " << total << " op/t [ ";
        for(i=0;i<threads;i++) {
            if(server->log & 64) std::cout << counts[i] << " ";
            if(counts[i] > counts[hi]) hi = i;
            if(counts[i] < counts[low]) low = i;
        }
        if(server->log & 64) std::cout << "]\n";

        if(!total || count_workers < 2) continue;

        int to_migrate = (counts[hi] - counts[low]) * count_workers / total / 2;
        if(!to_migrate) continue;

        {
            Lock lock(server);
            lock.lock(hi);

            int j=0;
            for(i=0;i<to_migrate;i++) {
                for(;j<=server->max_fd;j++) {
                    Connect *conn = server->connections[j];
                    if(!conn) continue;
                    if(conn->nloop != hi) continue;
                    if(!conn->counter) continue;
                    conn->need_loop = low;
                    break;
                }
            }
        }
    };
};
