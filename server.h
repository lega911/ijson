
#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include "utils.h"

class TcpServer;

class IConnect {
private:
    char _socket_status;  //  1 - read, 2 - write, -1 - closed
    int _link;
public:
    int fd;
    TcpServer *server;
    IConnect(int fd, TcpServer *server) : fd(fd) {
        _link = 0;
        _socket_status=1;
        this->server=server;
    };
    virtual ~IConnect() { fd = 0; };
    virtual void on_recv(char *buf, int size) {};
    virtual void on_send() {};
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
    std::vector<IConnect*> dead_connections;

    void start(Slice host, int port);
    void unblock_socket(int fd);
    void set_poll_mode(int fd, int status);  // 1 - read, 2- write, -1 closed
    
    virtual IConnect* on_connect(int fd) {return new IConnect(fd, this);};
    virtual void on_disconnect(IConnect *conn) {
        //delete conn;
    };
};


#endif /* SERVER_H */

