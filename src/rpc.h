
#ifndef RPC_H
#define RPC_H

#include <map>
#include <deque>

#include "server.h"
#include "utils.h"


using namespace std;

#define HTTP_START 0
#define HTTP_HEADER 1
#define HTTP_READ_BODY 2
#define HTTP_REQUEST_COMPLETED 3

#define STATUS_NET 21
#define STATUS_WAIT_JOB 22
#define STATUS_WAIT_RESPONSE 23
#define STATUS_WAIT_RESULT 24


class Connect : public IConnect {
private:
    int http_step;
    int content_length;
    int http_version;  // 10, 11
    Buffer buffer;

    Buffer path;
    Buffer name;
public:
    int status;
    Buffer body;
    Buffer id;
    bool fail_on_disconnect;
    bool noid;
    Connect *client;

    Connect(int fd, TcpServer *server) : IConnect(fd, server) {
        http_step = HTTP_START;
        status = STATUS_NET;
        client = NULL;
    }
    void on_recv(char *buf, int size);
    int read_method(Slice &line);
    void read_header(Slice &data);
    void send_details();
    void send_help();
    void rpc_add(ISlice params);

    void header_completed();
    void gen_id();
};

class MethodLine {
public:
    long last_worker;
    std::deque<Connect*> workers;
    std::deque<Connect*> clients;
};

class RpcServer : public TcpServer {
private:
    int _add_worker(ISlice name, Connect *worker);
public:
    std::map<std::string, MethodLine*> methods;
    std::map<std::string, Connect*> wait_response;
    std::vector<NetFilter> net_filter;
    
    RpcServer() : TcpServer() {};
    IConnect* on_connect(int fd, uint32_t ip) {
        if(net_filter.size()) {
            bool ok=false;
            for(int i=0;i<net_filter.size();i++) {
                if(net_filter[i].match(ip)) {
                    ok = true;
                    break;
                }
            }
            if(!ok) return NULL;
        }
        return new Connect(fd, this);
    };

    void on_disconnect(IConnect *conn);
    void add_worker(ISlice name, Connect *worker);
    int client_request(ISlice name, Connect *client, Slice id);
    int worker_result(ISlice id, Connect *worker);
    int worker_result_noid(Connect *worker);
};


#endif /* RPC_H */
