
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
#include "balancer.h"


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


void Server::_listen() {
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


bool Server::_valid_ip(u32 ip) {
    if(!net_filter.size()) return true;

    for(int i=0;i<net_filter.size();i++) {
        if(net_filter[i].match(ip)) {
            return true;
        }
    }
    return false;
}


void Server::_accept() {
    connections = (Connect**)_malloc(sizeof(Connect*) * MAX_EVENTS);
    if(connections == NULL) throw error::NoMemory();
    memset(connections, MAX_EVENTS, sizeof(Connect*));

    int balance = 0;
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

        if(fd > max_fd) max_fd = fd;
        if(log & 16) std::cout << ltime() << "connect " << fd << " " << (void*)conn << std::endl;

        loops[balance]->accept(conn);
        balance++;
        if(balance >= threads) balance = 0;
    }
};

void Server::start() {
    _listen();

    if(threads < 1) threads = 1;
    if(threads > 62) {
        threads = 62;
        if(log & 4) std::cout << "max threads is 62\n";
    }
    loops = (Loop**)_malloc(sizeof(Loop*) * threads);

    for(int i=0; i<threads; i++) {
        Loop *loop = new Loop(this, i);
        loop->start();
        loops[i] = loop;
    }

    Balancer balancer(this);
    if(threads > 1) balancer.start();

    _accept();
};


Lock Server::autolock(int except) {
    Lock lock(this);
    for(int i=0;i<threads;i++) {
        if(i != except) lock.lock(i);
    }
    return lock;
}


Queue *Server::get_queue(std::string &key, bool create) {
    auto it = _queue.find(key);
    if(it != _queue.end()) return it->second;
    if(!create) return NULL;

    Queue *q;
    LOCK _l(global_lock);
    it = _queue.find(key);
    if(it != _queue.end()) q = it->second;
    else {
        q = new Queue();
        _queue[key] = q;
    }
    return q;
}


/* Loop */

Loop::Loop(Server *server, int nloop) {
    accept_request = false;
    this->server = server;
    _nloop = nloop;
};


void Loop::start() {
    _thread = std::thread(&Loop::_loop_safe, this);
}


void Loop::accept(Connect *conn) {
    eitem event = {0};
    event.data.fd = conn->fd;

    conn->nloop = conn->need_loop = _nloop;
    conn->loop = this;

    int st = conn->get_socket_status();
    if(st == -1) throw Exception("accept: connection is closed");
    if(st & 1) event.events |= EPOLLIN;
    if(st & 2) event.events |= EPOLLOUT;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->fd, &event) < 0) {
        throw Exception("epoll_ctl EPOLL_CTL_ADD");
    }
}


void Loop::wake() {
    if(!server->fake_fd) {
        LOCK _l(server->global_lock);
        if(!server->fake_fd) server->fake_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(server->fake_fd < 0) throw Exception("Error opening socket");
    }

    eitem event = {0};
    event.data.fd = server->fake_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server->fake_fd, &event) < 0) {
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

        bool need_to_migrate = false;
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
            if(fd == server->fake_fd) {
                set_poll_mode(fd, -1);
                continue;
            }

            if(events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
                if(server->log & 2) std::cout << "epoll_wait returned EPOLLERR/EPOLLHUP (" << events[i].events << "): " << fd << std::endl;
                //IConnect* conn = this->connections[fd];
                //conn->on_error();
                _close(fd);
                continue;
            }

            Connect* conn = server->connections[fd];
            if(conn->nloop != _nloop) {
                if(server->log & 1) std::cout << "loop warning: connection is in wrong loop\n";
                continue;
            }
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
                    if(conn->is_closed()) _close(fd);
                }
            } else if (events[i].events & EPOLLOUT) {
                try {
                    conn->on_send();
                } catch (const Exception &e) {
                    if(server->log & 2) std::cout << ltime() << "Exception on_send: " << e.what() << std::endl;
                    conn->close();
                }
                if(conn->is_closed()) _close(fd);
            }

            if(conn->go_loop) need_to_migrate = true;
        }

        if(need_to_migrate) {
            Lock lock = server->autolock(_nloop);

            for (int i = 0; i <= server->max_fd; i++) {
                Connect* conn = server->connections[i];
                if(!conn) continue;
                if(conn->nloop != _nloop) continue;
                if(!conn->go_loop || conn->need_loop == _nloop) continue;
                conn->go_loop = false;
                if(conn->is_closed()) continue;
                set_poll_mode(conn->fd, -1);
                if(server->log & 64) std::cout << "migrate fd " << conn->fd << ", " << _nloop << " -> " << conn->need_loop << std::endl;
                auto loop = server->loops[conn->need_loop];
                loop->accept(conn);

                if(conn->status == STATUS_MIGRATE_REQUEST) {
                    loop->accept_request = true;
                    loop->wake();
                }
            }
        }

        // delete dead connections
        if(dead_connections.size()) {
            if(del_lock.try_lock()) {
                for(Connect *conn : dead_connections) {
                    if(conn->get_link() == 0) {
                        if(server->log & 16) std::cout << ltime() << "delete connection " << (void*)conn << std::endl;
                        delete conn;
                    }
                }
                dead_connections.clear();
                del_lock.unlock();
            }
        }

        if(accept_request) {
            Lock lock = server->autolock(_nloop);
            accept_request = false;
            for (int i = 0; i <= server->max_fd; i++) {
                Connect* conn = server->connections[i];
                if(!conn || conn->nloop != _nloop) continue;
                if(conn->status != STATUS_MIGRATE_REQUEST) continue;
                if(conn->is_closed()) continue;

                client_request(conn->name, conn);
            }
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
    worker->counter++;
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

    Queue *q = server->get_queue(key, true);

    //std::cout << "lock: add_worker\n";  // DEBUG

    Connect *client = NULL;
    int result;
    {
    LOCK _lock(q->mutex);
    q->last_worker = get_time_sec();
    std::string sid;
    while(q->clients.size()) {
        client = q->clients.front();
        q->clients.pop_front();
        client->unlink();

        if(client->is_closed()) {
            if(server->log & 8) std::cout << ltime() << "closed client " << client << std::endl;;
            client = NULL;
            continue;
        }
        if(client->status != CLIENT_WAIT_RESULT) {
            if(server->log & 8) std::cout << ltime() << "client is busy!!! " << client << std::endl;;
            client = NULL;
            continue;
        }

        LOCK _lc(client->mutex);
        if(client->status != CLIENT_WAIT_RESULT) {
            if(server->log & 8) std::cout << ltime() << "client is busy!!! " << client << std::endl;;
            client = NULL;
            continue;
        }
        //std::cout << "client: busy\n";  // DEBUG
        client->status = CONNECT_BUSY;

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
        if(server->wait_response[sid] != NULL) {
            // colision id
            if(server->log & 2) std::cout << ltime() << "collision id\n";
            client->send.status("400 Collision Id")->done(-1);  // FIXME
            client->status = STATUS_NET;
            client = NULL;
            continue;
        }
        break;
    }

    if(client) {
        //std::cout << "client foundi in queue\n";  // DEBUG
        if(worker->fail_on_disconnect) {
            worker->client = client;
            worker->client->link();
        }
        if(worker->noid) {
            worker->status = STATUS_WAIT_RESULT;
            worker->send.status("200 OK")->header("Method", name)->autosend(false)->done(client->body);
        } else {
            worker->send.status("200 OK")->header("Id", client->id)->header("Method", name)->autosend(false)->done(client->body);
            server->wait_response[sid] = client;
            client->link();
            worker->status = STATUS_NET;
        }
        //std::cout << "unlock: add_worker\n";  // DEBUG
        result = 1;
    } else {
        //std::cout << "no client in queue\n";  // DEBUG
        if (q->workers.size()) {
            auto it=q->workers.begin();
            while (it != q->workers.end()) {
                Connect *conn = *it;
                if(conn->is_closed() || conn == worker) {
                    it = q->workers.erase(it);
                    conn->unlink();
                } else {
                    it++;
                }
            }
        }

        q->workers.push_back(worker);
        worker->link();
        worker->status = STATUS_WAIT_JOB;
        //std::cout << "unlock: add_worker\n";  // DEBUG
        result = 0;
    }

    }


    if(client && client->send_buffer.size()) {
        client->write_mode(true);
    }
    if(worker->send_buffer.size()) {
        worker->write_mode(true);
    }
    return result;
};

int Loop::client_request(ISlice name, Connect *client) {
    client->counter++;
    auto key = name.as_string();
    auto q = server->get_queue(key);
    if(!q) {
        if(server->log & 4) std::cout << ltime() << "404 no method " << key << std::endl;
        client->send.status("404 Not Found")->done(-32601);
        return -1;
    }


    Connect *worker = NULL;
    {
    LOCK queue_lock(q->mutex);
    
    //std::cout << "lock: clietn_request\n";  // DEBUG
    while (q->workers.size()) {
        worker = q->workers.front();
        q->workers.pop_front();
        worker->unlink();

        if(worker->is_closed()) {
            if(server->log & 8) std::cout << ltime() << "worker closed " << worker << std::endl;
            worker = NULL;
            continue;
        }
        if(worker->status != STATUS_WAIT_JOB) {
            std::cout << "worker is not ready " << worker << std::endl;
            worker = NULL;
            continue;
        }

        LOCK _lw(worker->mutex);
        if(worker->status != STATUS_WAIT_JOB) {
            std::cout << "worker is not ready " << worker << std::endl;
            worker = NULL;
            continue;
        }
        worker->status = CONNECT_BUSY;
        break;
    };
    if(worker) {
        //std::cout << "worker found\n";  // DEBUG
        if(worker->noid) {
            worker->client = client;
            client->link();
            //if(worker->nloop != _nloop) LOCK _lw(worker->mutex);

            worker->status = STATUS_WAIT_RESULT;
            worker->send.status("200 OK")->header("Method", name)->autosend(false)->done(client->body);
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
            if(server->wait_response[sid] != NULL) {
                q->workers.push_front(worker);
                worker->link();
                worker->status = STATUS_WAIT_JOB;
                if(server->log & 4) std::cout << ltime() << "400 collision id " << key << std::endl;
                client->send.status("400 Collision Id")->done(-1);  // FIXME
                return -3;
            }
            if(worker->fail_on_disconnect) {
                worker->client = client;
                client->link();
            }
            worker->send.status("200 OK")->header("Id", id)->header("Method", name)->done(client->body);
            server->wait_response[sid] = client;
            client->link();
            worker->status = STATUS_NET;
        }
    } else {
        /*if(server->threads > 1) {
            int move_to = _nloop;
            for(int i=0;i<server->threads;i++) {
                if(i == _nloop) continue;
                Loop *l = server->loops[i];

                l->del_lock.lock();
                MethodLine *ml = l->methods[key];
                if(ml->workers.size()) move_to = i;
                l->del_lock.unlock();
                if(move_to != _nloop) break;
            }

            if(move_to != _nloop) {
                // DEBUG
                std::cout << "move request " << client->fd << ", " << _nloop << " -> " << move_to << std::endl;

                client->name.set(name);
                client->need_loop = move_to;
                client->go_loop = true;
                client->status = STATUS_MIGRATE_REQUEST;
                return 7;
            }
        }*/

        //std::cout << "no worker\n";
        q->clients.push_back(client);
        client->link();
    }

    //std::cout << "client: wait result 2\n";  // DEBUG
    client->status = CLIENT_WAIT_RESULT;
    //std::cout << "unlock: clietn_request\n";  // DEBUG
    }


    if(worker && worker->send_buffer.size()) {
        worker->write_mode(true);
    }
    if(client->send_buffer.size()) {
        client->write_mode(true);
    }

    return 0;
};

int Loop::worker_result(ISlice id, Connect *worker) {
    auto it = server->wait_response.find(id.as_string());
    if(it == server->wait_response.end()) return -1;
    Connect *client = it->second;
    server->wait_response.erase(it);

    client->unlink();
    if(client->is_closed()) return -2;
    if(worker) client->send.status("200 OK")->header("Id", id)->done(worker->body);
    else client->send.status("503 Service Unavailable")->header("Id", id)->done(-1);
    client->status = STATUS_NET;

    if(worker && worker->nloop != worker->need_loop) migrate(worker, client);
    return 0;
};

int Loop::worker_result_noid(Connect *worker) {
    auto client = worker->client;
    if(!client) throw error::NotImplemented("No connected client for noid");

    if(!worker->worker_mode) {
        worker->noid = false;
        worker->fail_on_disconnect = false;
    }
    worker->client = NULL;
    client->unlink();

    if(client->is_closed()) return -2;
    //std::cout << "client: status net\n";  // DEBUG
    client->status = STATUS_NET;
    client->send.status("200 OK")->done(worker->body);

    if(worker->nloop != worker->need_loop) migrate(worker, client);
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

void Loop::migrate(Connect *w, Connect *c) {
    if(_nloop == w->need_loop && server->log & 1) std::cout << "migrate warning: connection is on target loop\n";

    w->go_loop = true;
    c->go_loop = true;
    c->need_loop = w->need_loop;
    if(server->log & 64) std::cout << "migrate: loop " << _nloop << " -> " << w->need_loop << ", fd " << w->fd << ", " << c->fd << std::endl;
};
