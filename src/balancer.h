
#ifndef BALANCER_H
#define BALANCER_H

#include <thread>
#include "utils.h"

class CoreServer;


class Balancer {
private:
    std::thread _thread;
    CoreServer *server;
    void _start();
public:
    Balancer(CoreServer *server) : server(server) {};
    void start();
};


#endif
