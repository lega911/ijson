/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "utils.h"
#include <sys/time.h>
#include <arpa/inet.h>


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
    char *d = (char*)malloc(size + 1);
    memcpy(d, s, size);
    d[size] = 0;
    long t = get_time();
    std::cout << t - last_time << ": " << title << " " << d << std::endl;
    last_time = t;
    free(d);
}

void print2(const char *title, Buffer &b) {
    print2(title, b.ptr(), b.size());
}

void print2(const char *title) {
    long t = get_time();
    std::cout << t - last_time << ": " << title << std::endl;
    last_time = t;
}

NetFilter::NetFilter(Slice &mask) {
    std::string ip;
    int bits = 32;
    for(int i=0;i<mask.size();i++) {
        if(mask.ptr()[i] == '/') {
            ip.append(mask.ptr(), i);
            mask.remove(i + 1);
            bits = mask.atoi();
            break;
        }
    };
    if(ip.empty()) ip = mask.as_string();
    _ip = inet_addr(ip.c_str());

    _mask = 0xffffffff;
    bits = 32 - bits;
    for(int i=0;i<bits;i++) _mask = _mask << 1;
    // swap bytes
    char *b4 = (char*)&_mask;
    char t = b4[0]; b4[0] = b4[3]; b4[3] = t;
    t = b4[1]; b4[1] = b4[2]; b4[2] = t;

    _ip = _ip & _mask;
}

bool NetFilter::match(uint32_t ip) {
    return (ip & _mask) == _ip;
}
