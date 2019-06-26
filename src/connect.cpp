
#include <string.h>
#include <sys/socket.h>
#include <string.h>
#include <uuid/uuid.h>
#include "connect.h"


void Connect::unlink() {
    _link--;
    if(_link == 0) loop->dead_connections.push_back(this);
    else if(_link < 0) throw Exception("Wrong link count");
};

void Connect::write_mode(bool active) {
    if(active) {
        if(_socket_status & 2) return;
        _socket_status |= 2;
    } else {
        if(!(_socket_status & 2)) return;
        _socket_status = _socket_status & 0xfd;
    }
    loop->set_poll_mode(fd, _socket_status);
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


void Connect::on_recv(char *buf, int size) {
    if(!(status == STATUS_NET || (status == STATUS_WAIT_RESULT && noid))) {
        if(server->log & 4) std::cout << "warning: data is come, but connection is not ready\n";
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
                    if(status == STATUS_NET) {
                        http_step = HTTP_START;
                        continue;
                    } else {
                        throw error::NotImplemented("previous request is not finished");
                        // buffer.set(data);
                        // cout << "warning: previous request is not finished\n";
                        // break;
                    }
                }
            }
            break;
        }
        if(http_step == HTTP_START) {
            body.clear();
            id.clear();
            name.clear();
            this->jdata.reset();
            content_length = 0;
            if(status != STATUS_WAIT_RESULT) {
                fail_on_disconnect = false;
                if(client) client->unlink();
                client = NULL;
                noid = false;
            } else {
                if(!noid) throw error::NotImplemented("noid is false");
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
    } else if(data.starts_with("Name: ")) {
        data.remove(6);
        name.set(data);
    } else if(data.starts_with("Id: ") || data.starts_with("id: ")) {
        data.remove(4);
        id.set(data);
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
    
    if(this->path.equal("echo")) {
        Slice response("ok");
        this->send.status("200 OK")->done(response);
        return;
    }

    #ifdef DEBUG
    if(this->path.equal("debug")) {
        Buffer r(32);
        r.add("Memory allocated: ");
        r.add_number(get_memory_allocated());
        r.add("\n");
        this->send.status("200 OK")->done(r);
        return;
    }
    #endif

    if(noid) {
        if(!this->path.equal("rpc/result")) {
            this->send.status("400 Result expected")->done(-1);
            return;
        };
        counter++;
        int r = loop->worker_result_noid(this);
        if(r == 0) {
            this->send.status("200 OK")->done(1);
        } else if(r == -2) {
            if(server->log & 4) std::cout << ltime() << "499 Client is gone\n";
            this->send.status("499 Closed")->done(-1);
        } else throw error::NotImplemented("Wrong result for noid");
        status = STATUS_NET;
        return;
    }

    Slice method;
    Slice id(this->id);
    jdata.parse(this->body);

    if(this->path.equal("rpc/call")) {
        method = jdata.get_method();
        if(!method.empty() && method.ptr()[0] == '/') method.remove(1);
        if(method.empty()) {
            this->send.status("400 No method")->done(-32601);
            return;
        }
    } else {
        method = this->path;
    };

    if(method.equal("rpc/add")) {
        rpc_add();
        return;
    } else if(method.equal("rpc/result")) {
        if(id.empty()) id = jdata.get_id();
        if(id.empty()) {
            if(server->log & 2) std::cout << ltime() << "400 no id for /rpc/result\n";
            this->send.status("400 No id")->done(-1);
        } else {
            counter++;
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
        status = STATUS_NET;
        return;
    } else if(method.equal("rpc/details")) {
        send_details();
        return;
    } else if(method.equal("/") || method.equal("rpc/help")) {
        send_help();
        return;
    }

    int r = loop->client_request(method, this);
    if(r == 0) {
        status = STATUS_WAIT_RESPONSE;
    } else if(r == -1) {
        if(server->log & 4) std::cout << ltime() << "404 no method " << method.as_string() << std::endl;
        this->send.status("404 Not Found")->done(-32601);
    // } else if(r == -2) {
    //    this->send("400 No Id");
    } else if(r == -3) {
        if(server->log & 4) std::cout << ltime() << "400 collision id " << method.as_string() << std::endl;
        this->send.status("400 Collision Id")->done(-1);
    } else {
        throw error::NotImplemented();
    }
}

void Connect::rpc_add() {
    Slice name(this->name);
    if(name.empty()) name = this->jdata.get_name();
    if(this->id.equal("false")) {
        this->noid = true;
        this->fail_on_disconnect = true;
    } else {
        this->noid = this->jdata.get_noid();
        this->fail_on_disconnect = this->noid || jdata.get_fail_on_disconnect();
    }

    if(name.empty()) {
        this->send.status("400 No name")->done(-32602);
    } else {
        loop->add_worker(name, this);
    }
}

void Connect::gen_id() {
    id.resize(36, 36);
    uuid_t uuid;
    uuid_generate_time_safe(uuid);
    uuid_unparse_lower(uuid, id.ptr());
}


void Connect::send_details() {
    Buffer res(256);
    res.add("{");
    for(const auto &it : loop->methods) {
        std::string name = it.first;
        MethodLine *ml = it.second;
        res.add("\"");
        res.add(name);
        res.add("\":{\"last_worker\":");
        res.add_number(ml->last_worker);
        res.add(",\"workers\":");
        res.add_number(ml->workers.size());
        res.add(",\"clients\":");
        res.add_number(ml->clients.size());
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
    res.add("\n\nrpc/add\nrpc/result\nrpc/details\nrpc/help\n\n");
    for(const auto &it : loop->methods) {
        std::string name = it.first;
        MethodLine *ml = it.second;
        res.add(name);
        res.add("  x ");
        res.add_number(ml->workers.size());
        res.add("\n");
    }
    send.status("200 OK")->done(res);
}


/* HttpSender */

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
