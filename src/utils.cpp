
#include "utils.h"
#include <sys/time.h>
#include <ctime>
#include "server.h"
#include "connect.h"


u64 get_time() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

u32 get_time_sec() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec;
}


Buffer lbuf(22);

const char *ltime() {
    // thread unsafe
    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);

    lbuf.clear();
    lbuf.add_number(now->tm_year + 1900);
    lbuf.add("-", 1);
    int mon = now->tm_mon + 1;
    if(mon < 10) lbuf.add("0", 1);
    lbuf.add_number(mon);
    lbuf.add("-", 1);
    if(now->tm_mday < 10) lbuf.add("0", 1);
    lbuf.add_number(now->tm_mday);
    lbuf.add(" ", 1);
    if(now->tm_hour < 10) lbuf.add("0", 1);
    lbuf.add_number(now->tm_hour);
    lbuf.add(":", 1);
    if(now->tm_min < 10) lbuf.add("0", 1);
    lbuf.add_number(now->tm_min);
    lbuf.add(":", 1);
    if(now->tm_sec < 10) lbuf.add("0", 1);
    lbuf.add_number(now->tm_sec);
    lbuf.add(" ", 1);

    return lbuf.ptr();
}


/* Lock  */

void Lock::lock(int n) {
    u64 v = 1 << n;
    if(mask & v) return;
    mask |= v;
    server->loops[n]->del_lock.lock();
}

void Lock::unlock() {
    u64 v = 1;
    for(int i=0;i<server->threads;i++) {
        if(v & mask) server->loops[i]->del_lock.unlock();
        v <<= 1;
    }
    mask = 0;
};


void generator_init() {
    std::srand(std::time(nullptr));
}


void generate_id(Buffer &r) {
    static u32 index = 0;
    r.clear();
    r.add_hex((++index << 16) + (std::rand() & 0xffff));
}


/* GC */

void GC::add(void *ptr, int type) {
    _list.push_back({ptr, type});
};

void GC::release() {
    for(auto const & i : _list) {
        switch(i.type) {
        case 1:
            {
                auto *it = (::Message*)i.ptr;
                delete it;
                break;
            }
        case 2:
            {
                auto *it = (::DirectMessage*)i.ptr;
                if(it->unlink() == 0) {
                    delete it;
                }
                break;
            }
        }
    }
    _list.clear();
};
