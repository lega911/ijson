/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "utils.h"
#include <sys/time.h>

long get_time() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

void Buffer::set(ISlice &s) {
    set(s.ptr(), s.size());
}

void Buffer::add(ISlice &s) {
    add(s.ptr(), s.size());
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
