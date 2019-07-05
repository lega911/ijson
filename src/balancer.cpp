
#include <unistd.h>
#include "balancer.h"
#include "server.h"
#include "connect.h"


void Balancer::start() {
    _thread = std::thread(&Balancer::_start, this);
}

void Balancer::_start() {
    int threads = server->threads;

    while(true) {
        usleep(500000);  // 500ms
        // will be implemented later
    };
};
