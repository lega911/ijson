
#include <iostream>
#include "rpc.h"


const char *help_info = "\
    --port 8001\n\
    --host 127.0.0.1\n\
    --filter 127.0.0.1/32\n\
    --log\n\
    --debug\n\
    --counter\n\
";

int main(int argc, char** argv) {
    int port = 8001;
    Buffer host;
    RpcServer server;

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
                NetFilter nf(next);
                server.net_filter.push_back(nf);
                i++;
                if(host.empty()) host.set("0.0.0.0");
            } else {
                std::cout << "Wrong port\n";
                return 0;
            }
        } else if(s.equal("--counter")) {
            server.counter_active = true;
        } else {
            std::cout << help_info;
            return 0;
        }
    }
    if(host.empty()) host.set("127.0.0.1");

    std::cout << "Start server!\n";
    try {
        server.start(host, port);
    } catch (const exception &e) {
        std::cout << "Fatal exception: " << e.what() << std::endl;
    }

    return 0;
}
