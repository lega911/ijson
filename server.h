
#ifndef SERVER_H
#define SERVER_H

#include "utils.h"

class TcpServer;

class IConnect {
private:
    char status;  //  1 - read, 2 - write, -1 - closed
public:
    int fd;
    TcpServer *server;
    IConnect(int fd, TcpServer *server) : fd(fd) {status=1; this->server=server;};
    virtual ~IConnect() {};
    virtual void on_recv(char *buf, int size) {};
    virtual void on_send() {};
    virtual void on_error() {};
    
    void write_mode(bool active);
    void read_mode(bool active);
    void close() {status = -1;};
    inline bool is_closed() {return status == -1;};
    
    int raw_send(const void *buf, uint size);
};

class TcpServer {
private:
    int port;
    int serverfd;
    int epollfd;
    IConnect** connections; // fixme

    void listen_socket();
    void init_epoll();
    void loop();
    
    void _close(int fd);
public:
    void start(int n_port);
    void unblock_socket(int fd);
    void set_poll_mode(int fd, int status);  // 1 - read, 2- write
    
    virtual IConnect* on_connect(int fd) {return new IConnect(fd, this);};
    virtual void on_disconnect(IConnect *conn) {delete conn;};
};


#endif /* SERVER_H */

