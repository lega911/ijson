
#include "utils.h"
#include <sys/time.h>
#include <ctime>


long get_time() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

long get_time_sec() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec;
}

long last_time = 0;

void print2(const char *title, const char *s, int size) {
    char *d = (char*)_malloc(size + 1);
    if(d == NULL) throw error::NoMemory();
    memcpy(d, s, size);
    d[size] = 0;
    long t = get_time();
    std::cout << t - last_time << ": " << title << " " << d << std::endl;
    last_time = t;
    _free(d);
}

void print2(const char *title, Buffer &b) {
    print2(title, b.ptr(), b.size());
}

void print2(const char *title) {
    long t = get_time();
    std::cout << t - last_time << ": " << title << std::endl;
    last_time = t;
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
