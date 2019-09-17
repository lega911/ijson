
#include <unistd.h>
#include <time.h>
#include "service.h"
#include "server.h"
#include "connect.h"


void Service::start() {
    _thread = std::thread(&Service::_start, this);
}

u64 get_ntime() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return (u64)spec.tv_sec * 1'000'000'000 + (u64)spec.tv_nsec;
}

void Service::_free_memory() {
    if(!server->_free_list.size()) return;

    server->_free_lock.lock();
    std::vector<char*> v(server->_free_list);
    server->_free_list.clear();
    server->_free_lock.unlock();
    usleep(10'000);
    for(auto const &ptr : v) _free(ptr);
}

void Service::_start() {
    int threads = server->threads;
    bool log = server->log & 64;
    u64 *used = new u64[threads]();
    int *cpu = new int[threads]();
    u64 prev_time = get_ntime();
    i16 clean_tasks = 2;

    while(true) {
        usleep(500'000);  // 500ms
        _free_memory();

        if(--clean_tasks <= 0) {
            clean_tasks = 2;
            _clean_dead_tasks();
        }

        #ifndef _NOBALANCER
        if(threads > 1) {
            // balance
            u64 now = get_ntime();
            u64 frame = now - prev_time;
            prev_time = now;

            struct timespec thread_time;
            clockid_t threadClockId;

            if(log) std::cout << "cpu: ";
            u64 total = 0;
            for(int i=0; i<threads; i++) {
                auto id = server->loops[i]->get_id();
                pthread_getcpuclockid(id, &threadClockId);
                clock_gettime(threadClockId, &thread_time);
                u64 usage = thread_time.tv_sec * 1'000'000'000 + thread_time.tv_nsec;
                cpu[i] = (usage - used[i]) * 100 / frame;
                used[i] = usage;

                if(log) std::cout << cpu[i] << "% ";
            }
            if(log) std::cout << std::endl;

            int active = -1;
            int min = 0;
            for(int i=0;i<threads;i++) {
                if(cpu[i] < cpu[min]) min = i;
                if(cpu[i] > 80) continue;
                active = i;
                break;
            }

            if(active == -1) active = min;
            if(server->active_loop != active) {
                server->active_loop = active;
                if(log) std::cout << "Active loop: " << active << std::endl;
            }
        }
        #endif
    };

    delete[] used;
    delete[] cpu;
};


void Service::_clean_dead_tasks() {
    LOCK _l(server->global_lock);
    for(const auto &ql : server->_queue_list) {
        if(!ql->mutex.try_lock()) continue;

        for(int nloop=0; nloop < server->threads; nloop++) {
            Queue *q = &ql->queue[nloop];

            auto it=q->workers.begin();
            while(it != q->workers.end()) {
                Connect *conn = *it;
                if(conn->is_closed() || conn->status != Status::worker_wait_job) {
                    it = q->workers.erase(it);
                    conn->unlink();
                } else {
                    it++;
                }
            }

            it=q->clients.begin();
            while(it != q->clients.end()) {
                Connect *conn = *it;
                if(conn->is_closed() || conn->status != Status::client_wait_result) {
                    it = q->clients.erase(it);
                    conn->unlink();
                } else {
                    it++;
                }
            }
        }
        ql->mutex.unlock();
    }
};
