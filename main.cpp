
#include <iostream>
#include "rpc.h"


int main(int argc, char** argv) {
    /*
    --port 8001
    --host 127.0.0.1
    --filter 127.0.0.1/32
    --log
    --debug
    */

    int port = 8001;
    Buffer host("127.0.0.1");
    std::vector<int> net_filter;

    Slice s, next;
    for(int i=1;i<argc;i++) {
        s.set(argv[i]);
        if(i + 1 < argc) next.set(argv[i + 1]);
        else next.reset();

        if(s.equal("--port")) {
            if(next.valid()) {
                port = next.atoi();
                i++;
            } else {
                std::cout << "Wrong port\n";
                return 0;
            }
        } else if(s.equal("--host")) {
            if(next.valid()) {
                host.set(next);
                i++;
            } else {
                std::cout << "Wrong host\n";
                return 0;
            }
        } else if(s.equal("--filter")) {
            if(next.valid()) {
                std::cout << "filter " << next.as_string() << endl;
                i++;
            } else {
                std::cout << "Wrong port\n";
                return 0;
            }
        } else {
            std::cout << "Help\n";
            return 0;
        }
    }

    std::cout << "Start server!\n";
    RpcServer server;
    try {
        server.start(host, port);
    } catch (const char * str) {
        std::cout << "Exception: " << str << std::endl;
    }

    return 0;
}
