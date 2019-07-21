
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
        usleep(500'000);  // 500ms
        // balancer will be implemented later

        // free memory
        if(server->_free_list.size()) {
            server->_free_lock.lock();
            std::vector<char*> v(server->_free_list);
            server->_free_list.clear();
            server->_free_lock.unlock();
            usleep(10'000);
            for(auto const &ptr : v) _free(ptr);
        }
    };
};
