
#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <map>
#include <deque>
#include <thread>
#include "utils.h"
//#include "connect.h"


class Loop;
class Connect;

class CoreServer {
private:
    int _fd;
    void _listen();
    void _accept();
    bool _valid_ip(u32 ip);
public:
    Slice host;
    int log;
    int port;
    int threads;
    bool jsonrpc2;
    std::vector<NetFilter> net_filter;
    Connect **connections;
    Loop **loops;

    CoreServer() {
        log = 0;
        port = 8001;
        threads = 1;
        jsonrpc2 = false;
    };

    void start();

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
    std::thread _thread;

    void _loop();
    void _loop_safe();
    void _close(int fd);
public:
    CoreServer *server;
    std::vector<Connect*> dead_connections;

    Loop(CoreServer *server);
    void start();
    void accept(Connect *conn);
    void set_poll_mode(int fd, int status);

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
};












/////////////////

/*
class TcpServer;
class IConnect;


class IConnect {
private:
    char _socket_status;  //  1 - read, 2 - write, -1 - closed
    int _link;
public:
    HttpSender send;
    Buffer send_buffer;
    bool keep_alive;
    int fd;
    TcpServer *server;
    IConnect(int fd, TcpServer *server) : fd(fd) {
        _link = 0;
        _socket_status=1;
        this->server=server;
        send.set_connect(this);
    };
    virtual ~IConnect() {
        fd = 0;
        send.set_connect(NULL);
    };
    virtual void on_recv(char *buf, int size) {};
    virtual void on_send();
    virtual void on_error() {};
    
    void write_mode(bool active);
    void read_mode(bool active);
    void close() {_socket_status = -1;};
    inline bool is_closed() {return _socket_status == -1;};
    
    int raw_send(const void *buf, uint size);

    int get_link() { return _link; }
    void link() { _link++; };
    void unlink();
};

class TcpServer {
private:
    int _port;
    Slice _host;
    int serverfd;
    int epollfd;
    IConnect** connections; // fixme

    void listen_socket();
    void init_epoll();
    void loop();
    
    void _close(int fd);
public:
    std::thread _th1;
    int _fn;

    int log;
    std::vector<IConnect*> dead_connections;

    TcpServer() {
        log = 0;
    };

    void start(Slice host, int port);
    void unblock_socket(int fd);
    void set_poll_mode(int fd, int status);  // 1 - read, 2- write, -1 closed
    
    virtual IConnect* on_connect(int fd, uint32_t ip) {return new IConnect(fd, this);};
    virtual void on_disconnect(IConnect *conn) {};
};

 */

#endif /* SERVER_H */
