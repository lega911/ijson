
#pragma once

#ifndef _VERSION
#define _VERSION "0.0.0"
#endif

static const char *ijson_version = _VERSION;

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <vector>

#include "memory.h"
#include "exception.h"
#include "buffer.h"
#include "netfilter.h"

#define LOCK std::lock_guard<std::mutex>

long get_time();
long get_time_sec();
const char *ltime();

class Server;

class Lock {
private:
    Server *server;
    u64 mask = 0;
public:
    Lock(Server *server) : server(server) {};
    ~Lock() {unlock();};

    void lock(int n);
    void unlock();
};


class GC {
public:
    static const int Message = 1;
    static const int DirectMessage = 2;

    void add(void *ptr, int type);
    void release();
    GC() {};
    GC(void *ptr, int type) { add(ptr, type); };
    ~GC() { release(); };
private:
    struct _gc_item {
        void *ptr;
        int type;
    };
    std::vector<_gc_item> _list;
};


void generator_init();
void generate_id(Buffer &r);
