
#include <string.h>
#include <sys/socket.h>
#include <string.h>
#include <bits/stdc++.h> 
#include "connect.h"


void Connect::unlink() {
    _link--;
    if(_link == 0) loop->dead_connections.push_back(this);
    else if(_link < 0) THROW("Wrong link count");
};

void Connect::write_mode(bool active) {
    if(active) {
        if(_socket_status & 2) return;
        _socket_status |= 2;
    } else {
        if(!(_socket_status & 2)) return;
        _socket_status = _socket_status & 0xfd;
    }
    loop->_queue_to_send.push_back(this);
}

void Connect::read_mode(bool active) {
    if(active) {
        if(_socket_status & 1) return;
        _socket_status |= 1;
    } else {
        if(!(_socket_status & 1)) return;
        _socket_status = _socket_status & 0xfe;
    }
    loop->set_poll_mode(fd, _socket_status);
}

int Connect::raw_send(const void *buf, uint size) {
    int sent = ::send(this->fd, buf, size, 0);
    if(sent > 0) loop->stat_send += sent;
    return sent;
}

void Connect::on_send() {
    if(send_buffer.size()) {
        int sent = this->raw_send(send_buffer.ptr(), send_buffer.size());
        if(sent < 0) THROW("Not implemented: sent < 0");
        send_buffer.remove_left(sent);
    } else if(!direct_message.empty()) {
        if(server->log & 32) this->start_time = get_time();
        auto *dm = direct_message.front();
        direct_message.pop_front();
        GC gc(dm, GC::DirectMessage);

        int sent = this->raw_send(dm->data.ptr(), dm->data.size());
        if(sent < 0) THROW("Not implemented: sent < 0");

        if(sent != dm->data.size()) send_buffer.add(&dm->data.ptr()[sent], dm->data.size() - sent);
    }

    if(send_buffer.size() == 0) {
        if(this->keep_alive) {
            this->write_mode(false);
        } else {
            this->close();
        }
    }
};


void Connect::on_recv(char *buf, int size) {
    if(ping) ping_timeout = get_time_sec() + ping;
    if(!(status == Status::net || (status == Status::worker_wait_result && noid) || (status == Status::worker_mode_async && worker_mode))) {
        if(server->log & 4) std::cout << ltime() << "connect " << (void*)this << ", warning: data is come, but connection is not ready\n";
        buffer.add(buf, size);
        return;
    }
    Slice data;
    if(buffer.size()) {
        buffer.add(buf, size);
        data = Slice(buffer);
    } else {
        data = Slice(buf, size);
    }

    if(http_step == HTTP_READ_BODY) {
        int for_read = content_length - body.size();
        if(for_read > data.size()) {
            body.add(data);
            buffer.clear();
            return;
        } else {
            Slice s = data.pop(for_read);
            body.add(s);
            //buffer.add(data);
        }
        if(body.size() == content_length) {
            try {
                this->header_completed();
            } catch (const error::InvalidData &e) {
                if(server->log & 4) std::cout << ltime() << "Error: Invalid data/json, socket " << fd << " " << (void*)this << std::endl;
                this->send.status("400 Invalid data")->done(-32700);
            }
            http_step = HTTP_START;
        }
        if(data.empty()) return;
    }

    Slice line;
    while(true) {
        line = data.pop_line();
        if(!line.valid()) break;  // wait next package
        line.rstrip();

        if(line.empty()) {
            if(http_step != HTTP_HEADER) throw error::InvalidData("Wrong HTTP request");
            http_step = HTTP_REQUEST_COMPLETED;
            if(content_length) {
                int for_read = content_length;
                if(for_read > data.size()) for_read = data.size();
                Slice body_data = data.pop(for_read);
                body.set(body_data);

                if(body.size() < content_length) {
                    http_step = HTTP_READ_BODY;
                };
            };
            //if(data.size()) buffer.set(data);
            buffer.clear();
            if(http_step == HTTP_REQUEST_COMPLETED) {
                try {
                    this->header_completed();
                } catch (const error::InvalidData &e) {
                    this->send.status("400 Invalid data")->done(-32700);
                }

                if(data.size()) {
                    if(status == Status::net) {
                        http_step = HTTP_START;
                        continue;
                    } else THROW("previous request is not finished");
                }
            }
            break;
        }
        if(http_step == HTTP_START) {
            body.clear();
            id.clear();
            header_option.reset();
            info.reset();
            json.reset();
            if(!worker_mode) name.clear();
            content_length = 0;
            priority = 0;
            required_worker.clear();
            no_response = false;
            type = RequestType::none;
            if(status != Status::worker_wait_result) {
                if(worker_mode) {
                    if(status != Status::worker_mode_async) THROW("Wrong status for worker");
                } else {
                    fail_on_disconnect = false;
                    if(client) client->unlink();
                    client = NULL;
                    noid = false;
                }
            } else {
                if(!noid) THROW("noid is false");
            }
            if(this->read_method(line) != 0) {
                if(server->log & 2) std::cout << ltime() << "Wrong http header\n";
                this->close();
                return;
            }
            http_step = HTTP_HEADER;
        } else if(http_step == HTTP_HEADER) {
            try {
                this->read_header(line);
            } catch (const error::InvalidData &e) {
                this->send.status("400 Invalid data")->done(-32700);
            }
        }
    }

    if(http_step == HTTP_REQUEST_COMPLETED) {
        http_step = HTTP_START;
    } else if(http_step == HTTP_READ_BODY) {
    } else if(http_step == HTTP_START || http_step == HTTP_HEADER) {
        buffer.add(data);
    }
};


int Connect::read_method(Slice &line) {
    int start = 0;
    char *buf = line.ptr();
    int part = 0;
    int i = 0;
    path.clear();
    for(;i<line.size();i++) {
        if(buf[i] != ' ') continue;

        if(part == 1) {
            if(i>start+1 && buf[start] == '/') start++;
            path.set(&buf[start], i - start);
            break;
        }
        start = i + 1;
        part++;
    }
    if(buf[line.size() - 1] == '1') http_version = 11;
    else http_version = 10;

    if(path.empty()) return -1;
    return 0;
}

void Connect::_set_type(ISlice &name) {
    if(name == "get") {
        type = RequestType::get;
    } else if (name == "get+") {
        type = RequestType::get_plus;
    } else if (name == "worker") {
        type = RequestType::worker;
    } else if (name == "async") {
        type = RequestType::async;
    } else if (name == "pub") {
        type = RequestType::pub;
    } else if (name == "result") {
        type = RequestType::result;
    } else if (name == "create") {
        type = RequestType::create;
    } else if (name == "delete") {
        type = RequestType::del;
    } else throw error::InvalidData();
}

void Connect::read_header(Slice &data) {
    if(data.starts_with_lc("content-length: ")) {
        data.remove(16);
        content_length = data.atoi();
    } else if(data.starts_with_lc("type: ")) {
        data.remove(6);
        _set_type(data);
    } else if(data.starts_with_lc("x-type: ")) {
        data.remove(8);
        _set_type(data);
    } else if(data.starts_with_lc("name: ")) {
        data.remove(6);
        name.set(data);
    } else if(data.starts_with_lc("id: ")) {
        data.remove(4);
        id.set(data);
    } else if(data.starts_with_lc("option: ")) {
        data.remove(8);
        header_option = data;
    } else if(data.starts_with_lc("priority: ")) {
        data.remove(10);
        priority = data.atoi();
    } else if(data.starts_with_lc("worker-id: ")) {
        data.remove(11);
        required_worker.set(data);
    } else if(data.starts_with_lc("set-id: ")) {
        data.remove(8);
        connection_id.set(data);
    } else if(data.starts_with_lc("timeout: ")) {
        data.remove(9);
        ping = data.atoi();
        ping_timeout = get_time_sec() + ping;
    }
}


void Connect::header_completed() {
    this->loop->stat_request++;
    this->keep_alive = http_version == 11;

    if(server->log & 32) {
        Buffer repr(250);
        if(start_time) {
            i64 dur = (get_time() - start_time) / 1000;
            if(dur >= 10000) {
                repr.add_number(dur / 1000);
                repr.add("sec ");
            } else {
                repr.add_number(dur);
                repr.add("ms ");
            }
            start_time = 0;
        }
        if(this->body.size() > 150) {
            repr.add(this->body.ptr(), 147);
            repr.add("...");
        } else if(this->body.size()) {
            repr.add(this->body);
        }
        for(int i=0;i<repr.size();i++) {
            if(repr.ptr()[i] < 32) repr.ptr()[i] = '.';
        }
        repr.add("\n", 2);
        std::cout << ltime() << this->path.as_string() << " " << this->body.size() << "b " << repr.ptr();
    }
    
    if(this->path == "echo") {
        Slice response("ok");
        this->send.status("200 OK")->done(response);
        return;
    }


    #ifdef DEBUG
    if(this->path == "rpc/migrate") {
        this->send.status("200 OK")->done();
        this->need_loop = this->nloop + 1;
        if(this->need_loop >= server->threads) this->need_loop = 0;
        this->go_loop = true;
        return;
    }

    if(this->path == "debug") {
        Buffer r(32);
        r.add("Memory allocated: ");
        r.add_number(get_memory_allocated());
        r.add("\n");
        this->send.status("200 OK")->done(r);
        return;
    }
    #endif

    if(worker_mode) {
        if(status != Status::worker_mode_async) loop->worker_result_noid(this);
        if(header_option == "stop") {
            if(worker_item) worker_item->pop();
            worker_mode = false;
            this->send.status("200 OK")->done(1);
            status = Status::net;
            return;
        }
        //if(go_loop)  // TODO move worker
        rpc_add();
        return;
    }

    if(noid) {
        if(this->path != "rpc/result" && type != RequestType::result) {
            this->send.status("400 Result expected")->done(-1);
            return;
        };
        int r = loop->worker_result_noid(this);
        if(r == 0) {
            this->send.status("200 OK")->done(1);
        } else if(r == -2) {
            if(server->log & 4) std::cout << ltime() << "499 Client is gone\n";
            this->send.status("499 Closed")->done(-1);
        }
        status = Status::net;
        return;
    }

    Slice method;
    Slice id(this->id);
    json.load(this->body);

    if(type != RequestType::none) {
        name.set(path);
        switch(type) {
        case RequestType::async:
            no_response = true;
            loop->client_request(name, this);
            break;
        case RequestType::get_plus:
            this->noid = true;
            this->fail_on_disconnect = true;
        case RequestType::get:
            rpc_add();
            break;
        case RequestType::worker:
            worker_mode = true;
            rpc_add();
            break;
        case RequestType::pub:
            pub(name);
            break;
        case RequestType::result:
            if(id.empty() && path != "/") id.set(path);
            rpc_result(id);
            break;
        case RequestType::create:
            create_queue(path);
            break;
        case RequestType::del:
            delete_queue(path);
            break;
        default:
            this->send.status("400 Wrong type")->done(-32602);
        }
        return;
    }

    if(this->path == "rpc/call") {
        Slice params;
        while(json.scan()) {
            if(json.key == "method") method = json.value;
            else if(json.key == "id" && id.empty()) {
                id = json.value;
                this->id.set(id);
            } else if(json.key == "params") params = json.value;
        }
        if(!params.empty()) json.load(params, 1);

        if(!method.empty() && method.ptr()[0] == '/') method.remove(1);
        if(method.empty()) {
            this->send.status("400 No method")->done(-32601);
            return;
        }
    } else {
        method = this->path;
    };

    if(method == "rpc/worker") {
        worker_mode = true;
        rpc_add();
        return;
    } else if(method == "rpc/add") {
        rpc_add();
        return;
    } else if(method == "rpc/result") {
        rpc_result(id);
        return;
    } else if(method == "rpc/details") {
        send_details();
        return;
    } else if(method == "/" || method == "rpc/help") {
        send_help();
        return;
    }

    no_response = header_option == "async";
    loop->client_request(method, this);
}

void Connect::rpc_result(ISlice &id) {
    if(id.empty() && json.level == 0) {
        while(json.scan()) {
            if(json.key == "id") {
                id = json.value;
                break;
            }
        }
    }
    if(id.empty()) {
        if(server->log & 2) std::cout << ltime() << "400 no id for /rpc/result\n";
        this->send.status("400 No id")->done(-1);
    } else {
        loop->stat_result++;
        int r = loop->worker_result(id, this);
        if(r == 0) {
            this->send.status("200 OK")->done(1);
        } else if(r == -2) {
            if(server->log & 4) std::cout << ltime() << "499 Client is gone\n";
            this->send.status("499 Closed")->done(-1);
        } else {
            if(server->log & 2) std::cout << ltime() << "400 Wrong id for /rpc/result\n";
            this->send.status("400 Wrong id")->done(-1);
        }
    }
    status = Status::net;
}

void Connect::rpc_add() {
    Slice name(this->name);

    while(json.scan()) {
        if(json.key == "name") {
            json.decode_value(this->name);
            name = this->name;
        } else if(json.key == "info") {
            info = json.value;
        } else if(json.key == "option") {
            if(json.value == "no_id") {
                this->noid = true;
                this->fail_on_disconnect = true;
            } else if(json.value == "fail_on_disconnect") this->fail_on_disconnect = true;
            else {
                // TODO: Wrong option!!!
            }
        }
    }

    if(name.empty()) {
        worker_mode = false;
        this->send.status("400 No name")->done(-32602);
        if(server->log & 4) std::cout << ltime() << "No name for worker " << this << std::endl;
        return;
    };

    if(worker_mode) {
        this->noid = true;
        this->fail_on_disconnect = true;
    };

    loop->add_worker(name, this);
}

void Connect::send_details() {
    Buffer res(256);

    u64 stat_request = 0;
    u64 stat_call = 0;
    u64 stat_result = 0;
    u64 stat_recv = 0;
    u64 stat_send = 0;
    u64 stat_ioevent = 0;
    for(int i=0; i<server->threads; i++) {
        stat_request += server->loops[i]->stat_request;
        stat_call += server->loops[i]->stat_call;
        stat_result += server->loops[i]->stat_result;
        stat_recv += server->loops[i]->stat_recv;
        stat_send += server->loops[i]->stat_send;
        stat_ioevent += server->loops[i]->stat_ioevent;
    }

    res.add("{\"$info\":{");
    res.add("\"version\":\"");
    res.add(ijson_version);
    res.add("\",\"uptime\":");
    res.add_number(get_time_sec() - server->stat_starttime);
    res.add(",\"connect\":");
    res.add_number(server->stat_connect);
    res.add(",\"request\":");
    res.add_number(stat_request);
    res.add(",\"call\":");
    res.add_number(stat_call);
    res.add(",\"result\":");
    res.add_number(stat_result);
    res.add(",\"recv\":");
    res.add_number(stat_recv);
    res.add(",\"send\":");
    res.add_number(stat_send);
    res.add(",\"io_event\":");
    res.add_number(stat_ioevent);
    res.add("},\n");

    LOCK _l(server->global_lock);
    for(const auto &ql : server->_queue_list) {
        if(ql == NULL) continue;
        res.add("\"");
        res.add(ql->name);
        res.add("\":{\"last_worker\":");
        res.add_number(ql->last_worker);
        res.add(",\"worker_ids\":[");

        int worker_count = 0;
        int client_count = 0;
        bool first = true;
        for(int i=0;i<server->threads;i++) {
            for(const auto &w : ql->queue[i].workers) {
                if(first) {
                    first = false;
                    res.add("\"", 1);
                } else res.add(",\"", 2);
                if(w->connection_id) {
                    int start = res.size();
                    res.add(w->connection_id);
                    json::escape(res, start);
                }
                res.add("\"", 1);
                worker_count++;
            }
            client_count += ql->queue[i].clients.size();
        }
        res.add("],\"workers\":");
        res.add_number(worker_count);
        res.add(",\"clients\":");
        res.add_number(client_count);

        if(!ql->info.empty()) {
            res.add(",\"info\":\"");
            int start = res.size();
            res.add(ql->info);
            json::escape(res, start);
            res.add("\"");
        }
        res.add("},\n");
    };
    res.resize(0, res.size() - 2);
    res.add("}");
    send.status("200 OK")->done(res);
}


const char *help_msg = "\n\n\
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
rpc/details  - get details in json\n\n";

void Connect::send_help() {
    Buffer res(256);
    res.add("ijson ");
    res.add(ijson_version);
    res.add(help_msg);
    LOCK _l(server->global_lock);
    std::vector<QueueLine*> list;
    for(const auto &ql : server->_queue_list) {
        if(ql == NULL) continue;
        list.push_back(ql);
    }
    std::sort(list.begin(), list.end(), [](const auto& l,const auto& r) {
        return l->name.compare(r->name) < 0;
    });

    for(const auto &ql : list) {
        res.add(ql->name);

        int client_count = 0;
        int worker_count = 0;
        for(int i=0;i<server->threads;i++) {
            client_count += ql->queue[i].clients.size();
            worker_count += ql->queue[i].workers.size();
        }

        for(int i=ql->name.size();i<20;i++) res.add(" ", 1);
        res.add(" ", 1);
        res.add_number(client_count);
        res.add("/");
        res.add_number(worker_count);
        if(!ql->info.empty()) {
            res.add("  ");
            int start = res.size();
            res.add(ql->info);
        }
        res.add("\n");
    }
    send.status("200 OK")->done(res);
}

void Connect::pub(ISlice &name) {
    QueueLine *ql = server->get_queue(name);
    if(!ql) {
        if(server->log & 4) std::cout << ltime() << "404 no method " << name.as_string() << std::endl;
        this->send.status("404 Not Found")->done(-32601);
        return;
    }

    auto *dm = new DirectMessage();
    dm->link();
    Buffer *data = &dm->data;
    data->add("HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nName: ");
    data->add(name);
    data->add("\r\nAsync: true\r\n");
    if(body.size() == 0) {
        data->add("Content-Length: 0\r\n\r\n");
    } else {
        data->add("Content-Length: ");
        data->add_number(body.size());
        data->add("\r\n\r\n");
        data->add(body);
    };

    LOCK _l(ql->workers.lock);
    auto *item = ql->workers.head;
    for(;item;item=item->next) {
        auto *worker = item->conn;

        bool send = false;
        if(worker->status == Status::worker_wait_job) {
            worker->mutex.lock();
            if(worker->status == Status::worker_wait_job) {
                worker->status = Status::busy;
                send = true;
            }
            worker->mutex.unlock();
        };

        if(send) {
            if(worker->worker_mode) worker->status = Status::worker_mode_async;
            else worker->status = Status::net;

            dm->link();
            worker->direct_message.push_back(dm);
            worker->write_mode(true);
        } else if(worker->noid) {
            dm->link();
            worker->direct_message.push_back(dm);
        }
    }
    this->send.status("200 OK")->done(1);
    if(dm->unlink() == 0) delete dm;
};

void Connect::create_queue(Slice name) {
    if(name.empty()) {
        this->send.status("400 Wrong queue")->done(-32600);
        return;
    }
    if(name.ptr()[0] == '/') name.remove(1);
    server->get_queue(name, true);
    this->send.status("200 OK")->done(1);
};

void Connect::delete_queue(Slice name) {
    if(name.empty()) {
        this->send.status("400 Wrong queue")->done(-32600);
        return;
    }
    if(name.ptr()[0] == '/') name.remove(1);
    server->delete_queue(name);
    this->send.status("200 OK")->done(1);
};


/* HttpSender */

HttpSender *HttpSender::status(const char *status) {
    conn->send_buffer.resize(256);
    conn->send_buffer.add("HTTP/1.1 ");
    conn->send_buffer.add(status);
    conn->send_buffer.add("\r\n");
    if(conn->keep_alive) {
        conn->send_buffer.add("Connection: keep-alive\r\n");
    }
    return this;
};

HttpSender *HttpSender::header(const char *key, const ISlice &value) {
    conn->send_buffer.add(key);
    conn->send_buffer.add(": ");
    conn->send_buffer.add(value);
    conn->send_buffer.add("\r\n");
    return this;
};

void HttpSender::done(ISlice &body) {
    if(conn->is_closed()) THROW("Trying to send to closed socket");

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
    if(conn->is_closed()) THROW("Trying to send to closed socket");

    conn->send_buffer.add("Content-Length: 0\r\n\r\n");
    conn->write_mode(true);
};

void HttpSender::done(int error) {
    if(!conn->server->jsonrpc2) done();
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

