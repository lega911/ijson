
#include <string.h>
#include <sys/socket.h>
#include <string.h>
#include "connect.h"
#include <bits/stdc++.h> 


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
    return ::send(this->fd, buf, size, 0);
}

void Connect::on_send() {
    if(send_buffer.size()) {
        int sent = this->raw_send(send_buffer.ptr(), send_buffer.size());
        if(sent < 0) THROW("Not implemented: sent < 0");
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


void Connect::on_recv(char *buf, int size) {
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
            no_response = false;
            type.reset();
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
            this->read_header(line);
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

void Connect::read_header(Slice &data) {
    if(data.starts_with("Content-Length: ")) {
        data.remove(16);
        content_length = data.atoi();
    } else if(data.starts_with("Type: ")) {
        data.remove(6);
        type = data;
    } else if(data.starts_with("Name: ")) {
        data.remove(6);
        name.set(data);
    } else if(data.starts_with("Id: ") || data.starts_with("id: ")) {
        data.remove(4);
        id.set(data);
    } else if(data.starts_with("Option: ")) {
        data.remove(8);
        header_option = data;
    } else if(data.starts_with("Priority: ")) {
        data.remove(10);
        priority = data.atoi();
    }
}


void Connect::header_completed() {
    this->keep_alive = http_version == 11;

    if(server->log & 32) {
        Buffer repr(250);
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
        if(this->path != "rpc/result") {
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
    bool root_json = true;

    if(!type.empty()) {
        name.set(path);
        if(type == "async") {
            no_response = true;
            loop->client_request(name, this);
        } else if(type == "get+") {
            this->noid = true;
            this->fail_on_disconnect = true;
            rpc_add();
        } else if(type == "get") {
            rpc_add();
        } else if(type == "worker") {
            worker_mode = true;
            rpc_add();
        } else {
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
        if(!params.empty()) {
            root_json = false;
            json.load(params);
        }

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
        if(id.empty() && root_json) {
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
    res.add("{\"_version\":\"");
    res.add(ijson_version);
    res.add("\",\n");
    LOCK _l(server->global_lock);

    for(const auto &ql : server->_queue_list) {
        res.add("\"");
        res.add(ql->name);
        res.add("\":{\"last_worker\":");
        res.add_number(ql->last_worker);
        res.add(",\"workers\":");

        int worker_count = 0;
        int client_count = 0;
        for(int i=0;i<server->threads;i++) {
            worker_count += ql->queue[i].workers.size();
            client_count += ql->queue[i].clients.size();
        }
        res.add_number(worker_count);
        res.add(",\"clients\":");
        res.add_number(client_count);

        if(!ql->info.empty()) {
            res.add(",\"info\":\"");
            res.add(ql->info);
            res.add("\"");
        }
        res.add("},\n");
    };
    if(res.size() > 2) res.resize(0, res.size() - 2);
    res.add("}");
    send.status("200 OK")->done(res);
}


void Connect::send_help() {
    Buffer res(256);
    res.add("ijson ");
    res.add(ijson_version);
    res.add("\n\nrpc/add     {name, [option], [info]}\nrpc/result  {[id]}\nrpc/worker  {name, [info]}\nrpc/details\nrpc/help\n\n");
    LOCK _l(server->global_lock);
    std::vector<QueueLine*> list;
    for(const auto &ql : server->_queue_list) list.push_back(ql);
    std::sort(list.begin(), list.end(), [](const auto& l,const auto& r) {
        return l->name.compare(r->name) < 0;
    });

    for(const auto &ql : list) {
        res.add(ql->name);

        for(int i=ql->name.size();i<20;i++) res.add(" ", 1);
        res.add("  x ");

        int worker_count = 0;
        for(int i=0;i<server->threads;i++) worker_count += ql->queue[i].workers.size() - ql->queue[i].clients.size();
        res.add_number(worker_count);
        if(!ql->info.empty()) {
            res.add("  ");
            int start = res.size();
            res.add(ql->info);
            json::unescape(res, start);
        }
        res.add("\n");
    }
    send.status("200 OK")->done(res);
}


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
