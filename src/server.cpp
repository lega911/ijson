
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
#include <unistd.h>
#include "connect.h"


typedef struct epoll_event eitem;
#define MAX_EVENTS 16384
#define BUF_SIZE 16384


void unblock_socket(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw Exception("fcntl F_GETFL");
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw Exception("fcntl F_SETFL O_NONBLOCK");
    }
}


void CoreServer::_listen() {
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0) {
        throw Exception("Error opening socket");
    }

    int opt = 1;
    if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw Exception("setsockopt");
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(host.as_string().c_str());
    serv_addr.sin_port = htons(port);

    int r = bind(_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(r < 0) throw Exception("Error on binding, port is busy?");  // fix vscode highlighting

    if (listen(_fd, 64) < 0) {
        throw Exception("ERROR on listen");
    }
    if(this->log & 8) std::cout << ltime() << "Server started on " << host.as_string() << ":" << port << std::endl;
};


bool CoreServer::_valid_ip(u32 ip) {
    if(!net_filter.size()) return true;

    for(int i=0;i<net_filter.size();i++) {
        if(net_filter[i].match(ip)) {
            return true;
        }
    }
    return false;
}


void CoreServer::_accept() {
    connections = (Connect**)_malloc(sizeof(Connect*) * MAX_EVENTS);
    if(connections == NULL) throw error::NoMemory();
    memset(connections, MAX_EVENTS, sizeof(Connect*));

    while (true) {
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        int fd = accept(_fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
        if (fd < 0) {
            if(log & 1) std::cout << ltime() << "warning: accept error\n";
            continue;
        }
        if (fd >= MAX_EVENTS) {
            std::cout << "socket fd (" << fd << ") >= " << MAX_EVENTS << std::endl;
            throw Exception("socket fd error");
        }
        unblock_socket(fd);

        if(!_valid_ip(peer_addr.sin_addr.s_addr)) {
            close(fd);
            if(log & 8) std::cout << ltime() << "Client filtered\n";
            continue;
        };

        if(connections[fd]) throw Exception("Connection place is not empty");
        Connect* conn = new Connect(this, fd);
        connections[fd] = conn;
        conn->link();

        if(log & 16) std::cout << ltime() << "connect " << fd << " " << (void*)conn << std::endl;

        loops[0]->accept(conn);
    }

};

void CoreServer::start() {
    _listen();

    if(threads < 1) threads = 1;
    loops = (Loop**)_malloc(sizeof(Loop*) * threads);

    for(int i=0; i<threads; i++) {
        Loop *loop = new Loop(this);
        loop->start();
        loops[i] = loop;
    }

    _accept();
};


/* Loop */

Loop::Loop(CoreServer *server) {
    this->server = server;
};


void Loop::start() {
    _thread = std::thread(&Loop::_loop_safe, this);
}


void Loop::accept(Connect *conn) {
    eitem event = {0};
    event.data.fd = conn->fd;
    event.events |= EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->fd, &event) < 0) {
        throw Exception("epoll_ctl EPOLL_CTL_ADD");
    }
}


void Loop::_loop_safe() {
    try {
        _loop();
    } catch (const std::exception &e) {
        std::cout << ltime() << "Fatal exception: " << e.what() << std::endl;
    }
}

void Loop::_loop() {
    epollfd = epoll_create1(0);
    if (epollfd < 0) {
        throw Exception("epoll_create1");
    }

    eitem* events = (eitem*)_malloc(MAX_EVENTS * sizeof(eitem));
    if(events == NULL) throw error::NoMemory();

    char *buf = (char*)_malloc(BUF_SIZE);
    if(buf == NULL) throw error::NoMemory();
    while(true) {
        int nready = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if(nready == -1) {
            if(server->log & 1) std::cout << ltime() << "epoll_wait error: " << errno << std::endl;
            continue;
        }

        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
            if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                std::cout << "epoll_wait returned EPOLLERR/EPOLLHUP (" << events[i].events << "): " << fd << std::endl;
                //IConnect* conn = this->connections[fd];
                //conn->on_error();
                _close(fd);
                continue;
            }

            Connect* conn = server->connections[fd];
            if (events[i].events & EPOLLIN) {
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
                        if(server->log & 2) std::cout << ltime() << "Exception on_recv: " << e.what() << std::endl;
                        conn->close();
                    }
                    if(conn->is_closed()) {
                        _close(fd);
                        continue;
                    }
                }
            } else if (events[i].events & EPOLLOUT) {
                try {
                    conn->on_send();
                } catch (const Exception &e) {
                    if(server->log & 2) std::cout << ltime() << "Exception on_send: " << e.what() << std::endl;
                    conn->close();
                }
                if(conn->is_closed()) {
                    _close(fd);
                    continue;
                }
            }
        }

        // delete dead connections
        if(dead_connections.size()) {
            for(Connect *conn : dead_connections) {
                if(conn->get_link() == 0) {
                    if(server->log & 16) std::cout << ltime() << "delete connection " << (void*)conn << std::endl;
                    delete conn;
                }
            }
            dead_connections.clear();
        }
    }
}


void Loop::set_poll_mode(int fd, int status) {
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


void Loop::_close(int fd) {
    Connect* conn = server->connections[fd];
    if(server->log & 16) std::cout << ltime() << "disconnect socket " << fd << " " << (void*)conn << std::endl;
    if(conn == NULL) throw Exception("_close: connection is null");
    conn->close();
    this->on_disconnect(conn);
    conn->unlink();
    server->connections[fd] = NULL;
    this->set_poll_mode(fd, -1);
    close(fd);
}







void Loop::add_worker(ISlice name, Connect *worker) {
    char *ptr = name.ptr();
    int start = 0;
    int i = 0;
    Slice n;
    for(;i<name.size();i++) {
        if(ptr[i] == ',') {
            n.set(&ptr[start], i - start);
            start = i + 1;
            if(_add_worker(n, worker) == 1) {
                // worker is taken
                return;
            }
        }
    }
    if(start < name.size()) {
        n.set(&ptr[start], i - start);
        _add_worker(n, worker);
    }
}

int Loop::_add_worker(Slice name, Connect *worker) {
    if(!name.empty() && name.ptr()[0] == '/') name.remove(1);
    std::string key = name.as_string();
    MethodLine *ml = this->methods[key];
    if(ml == NULL) {
        ml = new MethodLine();
        this->methods[key] = ml;
    }
    ml->last_worker = get_time_sec();
    Connect *client = NULL;
    std::string sid;
    while(ml->clients.size()) {
        client = ml->clients.front();
        ml->clients.pop_front();
        client->unlink();
        if(client->is_closed() || client->status != STATUS_WAIT_RESPONSE) {
            // TODO: close wrong connection?
            if(server->log & 8) std::cout << ltime() << "dead client\n";
            client = NULL;
            continue;
        }
        if(worker->noid) break;
        Slice id = client->id;
        if(id.empty()) {
            id = client->jdata.get_id();
            if(id.empty()) {
                client->gen_id();
                id = client->id;
            } else {
                client->id.set(id);
            }
        }
        sid = id.as_string();
        if(wait_response[sid] != NULL) {
            // colision id
            if(server->log & 2) std::cout << ltime() << "collision id\n";
            client->send.status("400 Collision Id")->done(-1);
            client->status = STATUS_NET;
            client = NULL;
            continue;
        }
        break;
    }

    if(client) {
        if(worker->fail_on_disconnect) {
            worker->client = client;
            worker->client->link();
        }
        if(worker->noid) {
            worker->send.status("200 OK")->header("Method", name)->done(client->body);
            worker->status = STATUS_WAIT_RESULT;
        } else {
            worker->send.status("200 OK")->header("Id", client->id)->header("Method", name)->done(client->body);
            wait_response[sid] = client;
            client->link();
            worker->status = STATUS_NET;
        }
        return 1;
    } else {
        if (ml->workers.size()) {
            auto it=ml->workers.begin();
            while (it != ml->workers.end()) {
                Connect *conn = *it;
                if(conn->is_closed() || conn == worker) {
                    it = ml->workers.erase(it);
                    conn->unlink();
                } else {
                    it++;
                }
            }
        }

        ml->workers.push_back(worker);
        worker->link();
        worker->status = STATUS_WAIT_JOB;
        return 0;
    }
};

int Loop::client_request(ISlice name, Connect *client) {
    auto it = this->methods.find(name.as_string());
    if (it == this->methods.end()) return -1;
    MethodLine *ml = it->second;

    Connect *worker = NULL;
    while (ml->workers.size()) {
        worker = ml->workers.front();
        ml->workers.pop_front();
        worker->unlink();
        if(worker->is_closed() || worker->status != STATUS_WAIT_JOB) {
            // TODO: close wrong connection?
            if(server->log & 8) std::cout << ltime() << "dead worker\n";
            worker = NULL;
            continue;
        }
        break;
    };
    if(worker) {
        if(worker->noid) {
            worker->client = client;
            client->link();
            worker->send.status("200 OK")->header("Method", name)->done(client->body);
            worker->status = STATUS_WAIT_RESULT;
        } else {
            Slice id(client->id);
            if(id.empty()) {
                id = client->jdata.get_id();
                if(!id.empty()) client->id.set(id);
            }
            if(id.empty()) {
                client->gen_id();
                id.set(client->id);
            }
            std::string sid = id.as_string();
            if(wait_response[sid] != NULL) {
                ml->workers.push_front(worker);
                worker->link();
                return -3;
            }
            if(worker->fail_on_disconnect) {
                worker->client = client;
                client->link();
            }
            worker->send.status("200 OK")->header("Id", id)->header("Method", name)->done(client->body);
            wait_response[sid] = client;
            client->link();
            worker->status = STATUS_NET;
        }
    } else {
        ml->clients.push_back(client);
        client->link();
    }
    return 0;
};

int Loop::worker_result(ISlice id, Connect *worker) {
    auto it = wait_response.find(id.as_string());
    if(it == wait_response.end()) return -1;
    Connect *client = it->second;
    wait_response.erase(it);

    client->unlink();
    if(client->is_closed()) return -2;
    if(worker) client->send.status("200 OK")->header("Id", id)->done(worker->body);
    else client->send.status("503 Service Unavailable")->header("Id", id)->done(-1);
    client->status = STATUS_NET;

    return 0;
};

int Loop::worker_result_noid(Connect *worker) {
    auto client = worker->client;
    if(!client) throw error::NotImplemented("No connected client for noid");

    worker->noid = false;
    worker->fail_on_disconnect = false;
    worker->client = NULL;
    client->unlink();

    if(client->is_closed()) return -2;
    client->send.status("200 OK")->done(worker->body);
    client->status = STATUS_NET;

    return 0;
};

void Loop::on_disconnect(Connect *conn) {
    Connect *c = conn;
    if(!c->fail_on_disconnect) return;
    if(c->noid) {
        if(c->status == STATUS_WAIT_RESULT) {
            if(!c->client) throw Exception("No client");
            if(!c->client->is_closed()) c->client->send.status("503 Service Unavailable")->done(-1);
            c->client->status = STATUS_NET;
        } else if(c->client) {
            throw error::NotImplemented("Client is linked to pending worker");
        }
    } else if(c->client) {
        worker_result(c->client->id, NULL);
    }
    if(c->client) {
        c->client->unlink();
        c->client = NULL;
    };
};




/////////////////////////

/*

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

void push(int epollfd, int fd) {
    std::cout << "Thread started\n";

    #define M 1000000
    int status = 15;

    usleep(5 * M);
    std::cout << "Start sending\n";

    for(int i=0;i<1000;i++) {
        //int r = ::send(fd, &status, 4, 0);
        //std::cout << "send\n";

        eitem event2 = {0};
        event2.data.fd = fd;
        event2.events = 0;
        //event2.events = EPOLLIN;

        int err = epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event2);
        if (err < 0) {
            throw Exception("epoll_ctl EPOLL_CTL_ADD");
        }

        usleep(10000);
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
    };

    // test

    _fn = socket(AF_INET, SOCK_STREAM, 0);
    if(_fn <= 0) {
        throw Exception("/dev/null is not available");
    }
    //this->unblock_socket(_fn);

    //this->_th1 = thread(push, epollfd, _fn);
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
                int fd = events[i].data.fd;
                if(fd == _fn) {
                    //std::cout << "inner event\n";
                    this->set_poll_mode(fd, -1);
                    continue;
                }
                printf("epoll_wait returned EPOLLERR/EPOLLHUP (%d): %d\n", events[i].events, fd);
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

*/

