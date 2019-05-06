
#include <iostream>
#include "rpc.h"


const char *help_info = "\
    ijson 0.1.2\n\
\n\
    --host 127.0.0.1:8001\n\
    --filter 127.0.0.1/32\n\
    --log <option>\n\
    --counter\n\
\n\
    /rpc/add\n\
    /rpc/result\n\
    /rpc/details\n\
    /rpc/help\n\
\n\
    --log\n\
        -1 - all options\n\
        1 - exceptions, critical errors\n\
        2 - errors / filtered / invalid request / wrong http request / error parsing\n\
        4 - warnings\n\
        8 - info messages\n\
        16 - recv / send content\n\
";

int main(int argc, char** argv) {
    int port = 8001;
    Buffer host;
    RpcServer server;
    server.log = 15;

    Slice s, next;
    for(int i=1;i<argc;i++) {
        s.set(argv[i]);
        if(i + 1 < argc) next.set(argv[i + 1]);
        else next.reset();

        if(s.equal("--host")) {
            if(next.valid()) {
                Slice _h = next.split_left(':');
                host.set(_h);
                if(!next.empty()) {
                    try {
                        port = next.atoi();
                    } catch(const Exception &e) {
                        std::cout << "Wrong host option\n";
                        return 1;
                    }
                }
                i++;
            } else {
                std::cout << "Wrong host\n";
                return 1;
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
                return 1;
            }
        } else if(s.equal("--log")) {
            if(next.valid()) {
                try {
                    server.log = next.atoi();
                } catch(const Exception &e) {
                    std::cout << "Wrong log option\n";
                    return 1;
                }
                i++;
            } else {
                std::cout << "Wrong log option\n";
                return 1;
            }
        } else if(s.equal("--counter")) {
            server.counter_active = true;
        } else if(s.equal("--help")) {
            std::cout << help_info;
            return 0;
        } else {
            std::cout << "Wrong option (" << s.as_string() << ")\n\nuse --help\n";
            return 1;
        }
    }
    if(host.empty()) host.set("127.0.0.1");

    try {
        server.start(host, port);
    } catch (const exception &e) {
        std::cout << ltime() << "Fatal exception: " << e.what() << std::endl;
    }
    return 1;
}
