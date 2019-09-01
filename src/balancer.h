
#pragma once

#include <thread>
#include "utils.h"

class Server;


class Balancer {
private:
    std::thread _thread;
    Server *server;
    void _start();
    void _free_memory();
public:
    Balancer(Server *server) : server(server) {};
    void start();
};
