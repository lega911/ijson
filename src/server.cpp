
#include "server.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "rpc.h"


typedef struct epoll_event eitem;
#define MAX_EVENTS 16384
#define BUF_SIZE 16384


void TcpServer::listen_socket() {
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        throw Exception("Error opening socket");
    }

    int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw Exception("setsockopt");
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(_host.as_string().c_str());
    serv_addr.sin_port = htons(_port);

    int r = bind(serverfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(r < 0) throw Exception("Error on binding, port is busy?");  // fix vscode highlighting

    if (listen(serverfd, 64) < 0) {
        throw Exception("ERROR on listen");
    }
    if(this->log & 8) std::cout << ltime() << "Server started on " << _host.as_string() << ":" << _port << std::endl;
}

void TcpServer::unblock_socket(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw Exception("fcntl F_GETFL");
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw Exception("fcntl F_SETFL O_NONBLOCK");
    }
}

void TcpServer::init_epoll() {
    epollfd = epoll_create1(0);
    if (epollfd < 0) {
        throw Exception("epoll_create1");
    }

    eitem accept_event;
    accept_event.data.fd = serverfd;
    accept_event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &accept_event) < 0) {
        throw Exception("epoll_ctl EPOLL_CTL_ADD");
    }

}

void TcpServer::start(Slice host, int port) {
    this->connections = (IConnect**)_malloc(MAX_EVENTS * sizeof(IConnect*));
    if(this->connections == NULL) throw error::NoMemory();
    this->_host = host;
    this->_port = port;
    this->listen_socket();
    this->unblock_socket(serverfd);
    this->init_epoll();
    this->loop();
}


void TcpServer::set_poll_mode(int fd, int status) {
    if(status == -1) {
        if (epoll_ctl(this->epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            throw Exception("epoll_ctl EPOLL_CTL_DEL");
        }
        return;
    }

    eitem event = {0};
    event.data.fd = fd;
    if(status & 1) event.events |= EPOLLIN;
    if(status & 2) event.events |= EPOLLOUT;

    if (epoll_ctl(this->epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
        throw Exception("epoll_ctl EPOLL_CTL_MOD");
    }
}


void IConnect::write_mode(bool active) {
    if(active) {
        if(_socket_status & 2) return;
        _socket_status |= 2;
    } else {
        if(!(_socket_status & 2)) return;
        _socket_status = _socket_status & 0xfd;
    }
    server->set_poll_mode(fd, _socket_status);
}

void IConnect::read_mode(bool active) {
    if(active) {
        if(_socket_status & 1) return;
        _socket_status |= 1;
    } else {
        if(!(_socket_status & 1)) return;
        _socket_status = _socket_status & 0xfe;
    }
    server->set_poll_mode(fd, _socket_status);
}

int IConnect::raw_send(const void *buf, uint size) {
    return ::send(this->fd, buf, size, 0);
}

void IConnect::unlink() {
    _link--;
    if(_link == 0) this->server->dead_connections.push_back(this);
    else if(_link < 0) throw Exception("Wrong link count");
};

void IConnect::on_send() {
    if(send_buffer.size()) {
        int sent = this->raw_send(send_buffer.ptr(), send_buffer.size());
        if(sent < 0) throw error::NotImplemented("Not implemented: sent < 0");
        send_buffer.remove_left(sent);
    }

    if(send_buffer.size() == 0) {
        if(this->keep_alive) {
            this->write_mode(false);
        } else {
            this->close();
        }
    }
};

void TcpServer::loop() {
    eitem* events = (eitem*)_malloc(MAX_EVENTS * sizeof(eitem));
    if(events == NULL) throw error::NoMemory();

    char *buf = (char*)_malloc(BUF_SIZE);
    if(buf == NULL) throw error::NoMemory();
    int attempt = 10;
    while (attempt--) {
        int nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if(nready == -1) {
            if(log & 1) std::cout << ltime() << "epoll_wait error: " << errno << std::endl;
            continue;
        }
        attempt = 10;
        for (int i = 0; i < nready; i++) {
            if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                printf("epoll_wait returned EPOLLERR/EPOLLHUP (%d): %d\n", events[i].events, events[i].data.fd);
                int fd = events[i].data.fd;
                IConnect* conn=this->connections[fd];
                conn->on_error();
                _close(fd);
                continue;
            }

            if (events[i].data.fd == serverfd) {
                struct sockaddr_in peer_addr;
                socklen_t peer_addr_len = sizeof(peer_addr);
                int fd = accept(serverfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
                if (fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if(log & 1) std::cout << ltime() << "accept: EAGAIN, EWOULDBLOCK\n";
                    } else {
                        if(log & 1) std::cout << ltime() << "warning: accept error\n";
                        continue;
                    }
                } else {
                    this->unblock_socket(fd);
                    if (fd >= MAX_EVENTS) {
                        printf("socket fd (%d) >= %d", fd, MAX_EVENTS);
                        throw Exception("socket fd error");
                    }
                    
                    if(this->connections[fd]) {
                        throw Exception("connection item already in use");
                    }
                    IConnect* conn;
                    try {
                        conn = this->on_connect(fd, peer_addr.sin_addr.s_addr);
                    } catch (const Exception &e) {
                        if(log & 2) std::cout << ltime() << "Exception on_connect: " << e.what() << std::endl;
                        close(fd);
                        continue;
                    }
                    if(conn == NULL) {
                        close(fd);
                        if(log & 8) std::cout << ltime() << "Client filtered\n";
                        continue;
                    }
                    this->connections[fd] = conn;
                    conn->link();

                    if(log & 16) std::cout << ltime() << "connect " << fd << " " << (void*)conn << std::endl;

                    eitem event = {0};
                    event.data.fd = fd;
                    event.events |= EPOLLIN;
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) < 0) {
                        throw Exception("epoll_ctl EPOLL_CTL_ADD");
                    }
                }
            } else {
                if (events[i].events & EPOLLIN) {
                    int fd = events[i].data.fd;
                    IConnect* conn=this->connections[fd];

                    int size = recv(fd, buf, BUF_SIZE, 0);
                    if(size == 0) {
                        _close(fd);
                    } else if (size < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // data is not ready yet
                            continue;
                        } else {
                            throw Exception("recv error");
                        }
                    } else {
                        try {
                            conn->on_recv(buf, size);
                        } catch (const Exception &e) {
                            if(log & 2) std::cout << ltime() << "Exception on_recv: " << e.what() << std::endl;
                            conn->close();
                        }
                        if(conn->is_closed()) {
                            _close(fd);
                            continue;
                        }
                    }
                } else if (events[i].events & EPOLLOUT) {
                    // Ready for writing.
                    int fd = events[i].data.fd;
                    IConnect* conn=this->connections[fd];
                    try {
                        conn->on_send();
                    } catch (const Exception &e) {
                        if(log & 2) std::cout << ltime() << "Exception on_send: " << e.what() << std::endl;
                        conn->close();
                    }
                    if(conn->is_closed()) {
                        _close(fd);
                        continue;
                    }
                }
            }
            
        }

        // delete dead connections
        if(dead_connections.size()) {
            for(IConnect *conn : dead_connections) {
                if(conn->get_link() == 0) {
                    if(log & 16) std::cout << ltime() << "delete connection " << (void*)conn << std::endl;
                    delete conn;
                }
            }
            dead_connections.clear();
        }

    }
}

void TcpServer::_close(int fd) {
    IConnect* conn=this->connections[fd];
    if(log & 16) std::cout << ltime() << "disconnect socket " << fd << " " << (void*)conn << std::endl;
    if(conn == NULL) throw Exception("_close: connection is null");
    conn->close();
    this->on_disconnect(conn);
    conn->unlink();
    this->connections[fd] = NULL;
    this->set_poll_mode(fd, -1);
    close(fd);
}

HttpSender *HttpSender::status(const char *status) {
    if(conn == NULL) throw error::NotImplemented();

    conn->send_buffer.resize(256);
    conn->send_buffer.add("HTTP/1.1 ");
    conn->send_buffer.add(status);
    conn->send_buffer.add("\r\n");
    if(conn->keep_alive) {
        conn->send_buffer.add("Connection: keep-alive\r\n");
    }
    return this;
};

HttpSender *HttpSender::header(const char *key, ISlice &value) {
    if(conn == NULL) throw error::NotImplemented();

    conn->send_buffer.add(key);
    conn->send_buffer.add(": ");
    conn->send_buffer.add(value);
    conn->send_buffer.add("\r\n");
    return this;
};

void HttpSender::done(ISlice &body) {
    if(conn == NULL) throw error::NotImplemented();
    if(conn->is_closed()) throw Exception("Trying to send to closed socket");

    int body_size = body.size();
    if(body_size == 0) {
        conn->send_buffer.add("Content-Length: 0\r\n\r\n");
    } else {
        conn->send_buffer.add("Content-Length: ");
        conn->send_buffer.add_number(body_size);
        conn->send_buffer.add("\r\n\r\n");
        conn->send_buffer.add(body);
    }
    conn->write_mode(true);
};

void HttpSender::done() {
    if(conn == NULL) throw error::NotImplemented();
    if(conn->is_closed()) throw Exception("Trying to send to closed socket");

    conn->send_buffer.add("Content-Length: 0\r\n\r\n");
    conn->write_mode(true);
};

void HttpSender::done(int error) {
    if(conn == NULL) throw error::NotImplemented();
    if(!((RpcServer*)conn->server)->jsonrpc2) done();
    else {
        Slice msg;
        if(error == -32700) msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");
        else if(error == -32600) msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32600, \"message\": \"Invalid Request\"}, \"id\": null}");
        else if(error == -32601) msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32601, \"message\": \"Method not found\"}, \"id\": null}");
        else if(error == -32602) msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32602, \"message\": \"Invalid params\"}, \"id\": null}");
        else if(error == -1) msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32001, \"message\": \"Error, see http code\"}, \"id\": null}");
        else if(error == 1) msg.set("{\"jsonrpc\": \"2.0\", \"result\": true, \"id\": null}");
        else msg.set("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32603, \"message\": \"Internal error\"}, \"id\": null}");
        done(msg);
    }
};
