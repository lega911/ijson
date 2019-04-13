
#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>

#include "exception.h"
#include "buffer.h"


long get_time();
long get_time_sec();
void print2(const char *title, const char *s, int size);
void print2(const char *title, Buffer &b);
void print2(const char *title);


class NetFilter {
private:
    uint32_t _ip;
    uint32_t _mask;
public:
    NetFilter(Slice &mask);
    bool match(uint32_t ip);
};

#endif /* UTILS_H */

