
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


#define eitem struct epoll_event
#define MAX_EVENTS 16384
#define BUF_SIZE 16384


void TcpServer::listen_socket() {
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        throw "Error opening socket";
    }

    // This helps avoid spurious EADDRINUSE when the previous instance of this
    // server died.
    int opt = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw "setsockopt";
    }

    //sockaddr serv_addr;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(serverfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        throw "Error on binding, port is busy?";
    }

    if (listen(serverfd, 64) < 0) {
        throw "ERROR on listen";
    }
}

void TcpServer::unblock_socket(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw "fcntl F_GETFL";
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw "fcntl F_SETFL O_NONBLOCK";
    }
}

void TcpServer::init_epoll() {
    epollfd = epoll_create1(0);
    if (epollfd < 0) {
        throw "epoll_create1";
    }

    eitem accept_event;
    accept_event.data.fd = serverfd;
    accept_event.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &accept_event) < 0) {
        throw "epoll_ctl EPOLL_CTL_ADD";
    }

}

void TcpServer::start(int n_port) {
    this->connections = (IConnect**)calloc(MAX_EVENTS, sizeof(IConnect*));
    this->port = n_port;
    this->listen_socket();
    this->unblock_socket(serverfd);
    this->init_epoll();
    this->loop();
}


void TcpServer::set_poll_mode(int fd, int status) {
    if(status == -1) {
        if (epoll_ctl(this->epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
            throw "epoll_ctl EPOLL_CTL_DEL";
        }
        return;
    }

    eitem event = {0};
    event.data.fd = fd;
    if(status & 1) event.events |= EPOLLIN;
    if(status & 2) event.events |= EPOLLOUT;

    if (epoll_ctl(this->epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
        throw "epoll_ctl EPOLL_CTL_MOD";
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
    else if(_link < 0) throw "Wrong link count";
};

void TcpServer::loop() {
    eitem* events = (eitem*)calloc(MAX_EVENTS, sizeof(eitem));
    if (events == NULL) {
        throw "Unable to allocate memory for epoll_events";
    }

    char *buf = (char*)calloc(BUF_SIZE, 1);

    while (1) {
        int nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);
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
                int fd = accept(serverfd, (struct sockaddr*)&peer_addr, &peer_addr_len);
                if (fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("accept: EAGAIN, EWOULDBLOCK\n");
                    } else {
                        throw "accept error";
                    }
                } else {
                    this->unblock_socket(fd);
                    if (fd >= MAX_EVENTS) {
                        printf("socket fd (%d) >= %d", fd, MAX_EVENTS);
                        throw "Error";
                    }
                    
                    if(this->connections[fd]) {
                        throw "connection item already in use";
                    }
                    IConnect* conn;
                    try {
                        conn = this->on_connect(fd);
                    } catch (Exception &e) {
                        std::cout << "Exception on_connect: " << e.get_msg() << std::endl;
                        close(fd);
                        continue;
                    }
                    this->connections[fd] = conn;
                    conn->link();

                    std::cout << "connect " << fd << " " << (void*)conn << std::endl;

                    eitem event = {0};
                    event.data.fd = fd;
                    event.events |= EPOLLIN;
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) < 0) {
                        throw "epoll_ctl EPOLL_CTL_ADD";
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
                            throw "recv error";
                        }
                    } else {
                        try {
                            conn->on_recv(buf, size);
                        } catch (Exception &e) {
                            std::cout << "Exception on_recv: " << e.get_msg() << std::endl;
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
                    } catch (Exception &e) {
                        std::cout << "Exception on_send: " << e.get_msg() << std::endl;
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
                    std::cout << "delete " << (void*)conn << std::endl;
                    delete conn;
                }
            }
            dead_connections.clear();
        }

    }
}

void TcpServer::_close(int fd) {
    IConnect* conn=this->connections[fd];
    std::cout << "disconnect socket " << fd << " " << (void*)conn << std::endl;
    if(conn == NULL) throw "connection is null";
    conn->close();
    this->on_disconnect(conn);
    conn->unlink();
    this->connections[fd] = NULL;
    this->set_poll_mode(fd, -1);
    close(fd);
}
