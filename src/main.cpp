
#include <iostream>
#include "server.h"


const char *help_info = "\n\
    --host [ip][:port], default 127.0.0.1:8001\n\
    --filter 127.0.0.1/32\n\
    --log <option>\n\
    --jsonrpc2\n\
    --threads <number>\n\
\n\
    --help\n\
    --version\n\
\n\
    /rpc/add     {name, [option], [info]}\n\
    /rpc/result  {[id]}\n\
    /rpc/worker  {name, [info]}\n\
    /rpc/details\n\
    /rpc/help\n\
\n\
    --log\n\
        -1 - all options\n\
        1 - exceptions, critical errors\n\
        2 - errors / filtered / invalid request / wrong http request / error parsing\n\
        4 - warnings\n\
        8 - info messages\n\
        16 - connect, disconnect, delete connection\n\
        32 - recv / send content\n\
        64 - balancing info\n\
        128 - debugging info\n\
";


int main(int argc, char** argv) {
    #ifdef DEBUG
        catch_fatal();
    #endif

    Server server;
    server.log = 15;

    Slice s, next;
    for(int i=1;i<argc;i++) {
        s.set(argv[i]);
        if(i + 1 < argc) next.set(argv[i + 1]);
        else next.reset();

        if(s == "--host") {
            if(next.valid()) {
                Slice _h = next.split_left(':');
                if(!_h.empty()) server.host.set(_h);
                if(!next.empty()) {
                    try {
                        server.port = next.atoi();
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
        } else if(s == "--filter") {
            if(next.valid()) {
                NetFilter nf(next);
                server.net_filter.push_back(nf);
                i++;
                if(server.host.empty()) server.host.set("0.0.0.0");
            } else {
                std::cout << "Wrong port\n";
                return 1;
            }
        } else if(s == "--log") {
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
        } else if(s == "--help") {
            std::cout << "ijson " << ijson_version << std::endl;
            std::cout << help_info;
            return 0;
        } else if(s == "--jsonrpc2") {
            server.jsonrpc2 = true;
        } else if(s == "--version") {
            std::cout << ijson_version << std::endl;
            return 0;
        } else if(s == "--threads") {
            server.threads = -1;
            if(next.valid()) {
                try {
                    server.threads = next.atoi();
                } catch(const Exception &e) {}
                i++;
            };
            if(server.threads < 0 || server.threads > 32) {
                std::cout << "Wrong thread option\n";
                return 1;
            }
        } else {
            std::cout << "Wrong option (" << s.as_string() << ")\n\nuse --help\n";
            return 1;
        }
    }

    if(server.host.empty()) {
        #ifdef DOCKER
            server.host.set("0.0.0.0");
        #else
            server.host.set("127.0.0.1");
        #endif
    }

    try {
        server.start();
    } catch (const Exception &e) {
        if(server.log & 1) e.print("Exception in server.start");
    } catch (const std::exception &e) {
        if(server.log & 1) std::cout << ltime() << "Fatal exception: " << e.what() << std::endl;
    }
    return 1;
}
