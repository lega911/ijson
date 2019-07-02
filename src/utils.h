
#ifndef UTILS_H
#define UTILS_H

static const char *ijson_version = "0.3.1";

#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>

#include "memory.h"
#include "exception.h"
#include "buffer.h"
#include "netfilter.h"

long get_time();
long get_time_sec();
const char *ltime();


class Server;

class Lock {
private:
    Server *server;
    u64 mask;
public:
    Lock(Server *server) : server(server), mask(0) {};
    ~Lock() {unlock();};

    void lock(int n);
    void unlock();
};

#endif /* UTILS_H */

