
#include <iostream>
#include "server.h"


const char *help_info = "\n\
Start options:\n\
    --host [ip][:port], default 127.0.0.1:8001\n\
    --filter 127.0.0.1/32\n\
    --jsonrpc2\n\
    --threads <number>\n\
    --log <mask>\n\
        -1 - all options\n\
        1 - exceptions, critical errors\n\
        2 - errors / filtered / invalid request / wrong http request / error parsing\n\
        4 - warnings\n\
        8 - info messages\n\
        16 - connect, disconnect, delete connection\n\
        32 - recv / send content\n\
        64 - balancing info\n\
        128 - debugging info\n\
    --help\n\
    --version\n\
\n\
Type of request (header type/x-type):\n\
  \"type: get\" - get a task\n\
  \"type: get+\" - get a task with keep-alive\n\
  \"type: worker\" - worker mode\n\
  \"type: async\" - send a command async\n\
  \"type: pub\" - publish a message (send message to all workers)\n\
  \"type: result\" - result from worker to client\n\
  \"type: create\" - create a queue\n\
  \"type: delete\" - delete a queue\n\
\n\
Another options (header):\n\
  \"priority: 15\" - set priority for request\n\
  \"set-id: 15\" - set id for worker\n\
  \"worker-id: 15\" - call worker with specific id\n\
  \"timeout: 60\" - timeout for worker in sec\n\
\n\
rpc/details  - get details in json\n\
\n\
Example:\n\
  Get command:\n\
    curl localhost:8001/command -H 'type: get'\n\
  Call command with id 15:\n\
    curl localhost:8001/command -d '{\"id\": 15, \"name\": \"world\"}'\n\
  Send result with id 15:\n\
    curl localhost:8001/ -H 'type: result' -d '{\"id\": 15, \"result\": \"hello!\"}'\n\
";

int main(int argc, char** argv) {
    #ifdef DEBUG
        catch_fatal();
    #endif
    generator_init();

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
