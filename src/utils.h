
#pragma once

static const char *ijson_version = "0.3.7";

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>

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
