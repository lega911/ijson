
#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <thread>
#include "utils.h"


class Loop;
class Connect;


class CoreServer {
private:
    int _fd;
    void _listen();
    void _accept();
    bool _valid_ip(u32 ip);
public:
    int max_fd;
    Slice host;
    int log;
    int port;
    int threads;
    bool jsonrpc2;
    int fake_fd;
    std::vector<NetFilter> net_filter;
    Connect **connections;
    Loop **loops;
    std::mutex lock;

    CoreServer() {
        log = 0;
        port = 8001;
        threads = 1;
        jsonrpc2 = false;
        max_fd = 0;
        fake_fd = 0;
    };

    void start();
    void make_queue(std::string name);

    Lock autolock(int except=-1);
};


class MethodLine {
public:
    long last_worker;
    std::deque<Connect*> workers;
    std::deque<Connect*> clients;
};


class Loop {
private:
    int epollfd;
    int _nloop;
    std::thread _thread;

    void _loop();
    void _loop_safe();
    void _close(int fd);
public:
    bool accept_request;
    CoreServer *server;
    std::vector<Connect*> dead_connections;
    std::mutex del_lock;

    Loop(CoreServer *server, int nloop);
    void start();
    void accept(Connect *conn);
    void set_poll_mode(int fd, int status);
    void wake();

// rpc
private:
    int _add_worker(Slice name, Connect *worker);
public:
    std::map<std::string, MethodLine*> methods;
    std::map<std::string, Connect*> wait_response;
    std::vector<NetFilter> net_filter;

    void on_disconnect(Connect *conn);
    void add_worker(ISlice name, Connect *worker);
    int client_request(ISlice name, Connect *client);
    int worker_result(ISlice id, Connect *worker);
    int worker_result_noid(Connect *worker);
    void migrate(Connect *w, Connect *c);
};


#endif /* SERVER_H */
