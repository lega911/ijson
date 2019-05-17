
#include "rpc.h"
#include <string.h>
#include <uuid/uuid.h>
#include "utils.h"
#include "json.h"


void Connect::on_recv(char *buf, int size) {
    if(!(status == STATUS_NET || (status == STATUS_WAIT_RESULT && noid))) {
        if(server->log & 4) cout << "warning: data is come, but connection is not ready\n";
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
                this->send.status("400 Invalid data")->done();
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
                    this->send.status("400 Invalid data")->done();
                }

                if(data.size()) {
                    if(status == STATUS_NET) {
                        http_step = HTTP_START;
                        continue;
                    } else {
                        throw error::NotImplemented("previous request is not finished");
                        /*
                        buffer.set(data);
                        cout << "warning: previous request is not finished\n";
                        break;
                        */
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
                if(server->log & 2) cout << ltime() << "Wrong http header\n";
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

    if(this->server->log & 16) {
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
    
    if(this->path.equal("/echo")) {
        Slice response("ok");
        this->send.status("200 OK")->done(response);
        return;
    }

    #ifdef DEBUG
    if(this->path.equal("/debug")) {
        Buffer r(32);
        r.add("Memory allocated: ");
        r.add_number(get_memory_allocated());
        r.add("\n");
        this->send.status("200 OK")->done(r);
        return;
    }
    #endif

    RpcServer *server = (RpcServer*)this->server;
    if(noid) {
        if(!this->path.equal("/rpc/result")) {
            this->send.status("400 Result expected")->done();
            return;
        };
        int r = server->worker_result_noid(this);
        if(r == 0) {
            this->send.status("200 OK")->done();
        } else if(r == -2) {
            if(server->log & 4) std::cout << ltime() << "499 Client is gone\n";
            this->send.status("499 Closed")->done();
        } else throw error::NotImplemented("Wrong result for noid");
        status = STATUS_NET;
        return;
    }

    Slice method;
    Slice id(this->id);
    jdata.parse(this->body);

    if(this->path.equal("/rpc/call")) {
        method = jdata.get_method();
        if(method.empty()) {
            this->send.status("400 No method")->done();
            return;
        }
    } else {
        method = this->path;
    };

    if(method.equal("/rpc/add")) {
        rpc_add();
        return;
    } else if(method.equal("/rpc/result")) {
        if(id.empty()) id = jdata.get_id();
        if(id.empty()) {
            if(server->log & 2) std::cout << ltime() << "400 no id for /rpc/result\n";
            this->send.status("400 No id")->done();
        } else {
            int r = server->worker_result(id, this);
            if(r == 0) {
                this->send.status("200 OK")->done();
            } else if(r == -2) {
                if(server->log & 4) std::cout << ltime() << "499 Client is gone\n";
                this->send.status("499 Closed")->done();
            } else {
                if(server->log & 2) std::cout << ltime() << "400 Wrong id for /rpc/result\n";
                this->send.status("400 Wrong id")->done();
            }
        }
        status = STATUS_NET;
        return;
    } else if(method.equal("/rpc/details")) {
        send_details();
        return;
    } else if(method.equal("/") || method.equal("/rpc/help")) {
        send_help();
        return;
    }

    int r = server->client_request(method, this);
    if(r == 0) {
        status = STATUS_WAIT_RESPONSE;
    } else if(r == -1) {
        if(server->log & 4) std::cout << ltime() << "404 no method " << method.as_string() << std::endl;
        this->send.status("404 Not Found")->done();
    /*} else if(r == -2) {
        this->send("400 No Id");*/
    } else if(r == -3) {
        if(server->log & 4) std::cout << ltime() << "400 collision id " << method.as_string() << std::endl;
        this->send.status("400 Collision Id")->done();
    } else {
        throw error::NotImplemented();
    }
}

void Connect::rpc_add() {
    Slice name(this->name);
    if(name.empty()) name = this->jdata.get_name();
    this->noid = this->jdata.get_noid();
    this->fail_on_disconnect = this->noid || jdata.get_fail_on_disconnect();

    if(name.empty()) {
        this->send.status("400 No name")->done();
    } else {
        ((RpcServer*)server)->add_worker(name, this);
    }
}

void RpcServer::add_worker(ISlice name, Connect *worker) {
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

int RpcServer::_add_worker(ISlice name, Connect *worker) {
    std::string key = name.as_string();
    MethodLine *ml = this->methods[key];
    if(ml == NULL) {
        ml = new MethodLine();
        this->methods[key] = ml;
    }
    ml->last_worker = get_time_sec();
    Connect *client = NULL;
    string sid;
    while(ml->clients.size()) {
        client = ml->clients.front();
        ml->clients.pop_front();
        client->unlink();
        if(client->is_closed() || client->status != STATUS_WAIT_RESPONSE) {
            // TODO: close wrong connection?
            if(log & 8) std::cout << ltime() << "dead client\n";
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
            if(log & 2) std::cout << ltime() << "collision id\n";
            client->send.status("400 Collision Id")->done();
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

void Connect::gen_id() {
    id.resize(36, 36);
    uuid_t uuid;
    uuid_generate_time_safe(uuid);
    uuid_unparse_lower(uuid, id.ptr());
}

int RpcServer::client_request(ISlice name, Connect *client) {
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
            if(log & 8) std::cout << ltime() << "dead worker\n";
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
            if(id.empty()) id = client->jdata.get_id();
            if(id.empty()) {
                client->gen_id();
                id.set(client->id);
            }
            string sid = id.as_string();
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

int RpcServer::worker_result(ISlice id, Connect *worker) {
    auto it = wait_response.find(id.as_string());
    if(it == wait_response.end()) return -1;
    Connect *client = it->second;
    wait_response.erase(it);

    client->unlink();
    if(client->is_closed()) return -2;
    if(worker) client->send.status("200 OK")->header("Id", id)->done(worker->body);
    else client->send.status("503 Service Unavailable")->header("Id", id)->done();
    client->status = STATUS_NET;

    return 0;
};

int RpcServer::worker_result_noid(Connect *worker) {
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

void RpcServer::on_disconnect(IConnect *conn) {
    Connect *c = (Connect*)conn;
    if(!c->fail_on_disconnect) return;
    if(c->noid) {
        if(c->status == STATUS_WAIT_RESULT) {
            if(!c->client) throw Exception("No client");
            c->client->send.status("503 Service Unavailable")->done();
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

void Connect::send_details() {
    RpcServer *server = (RpcServer*)this->server;
    Buffer res(256);
    res.add("{");
    for(const auto &it : server->methods) {
        string name = it.first;
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
    RpcServer *server = (RpcServer*)this->server;
    Buffer res(256);
    res.add("ijson ");
    res.add(ijson_version);
    res.add("\n\n/rpc/add\n/rpc/result\n/rpc/details\n/rpc/help\n\n");
    for(const auto &it : server->methods) {
        string name = it.first;
        MethodLine *ml = it.second;
        res.add(name);
        res.add("  x ");
        res.add_number(ml->workers.size());
        res.add("\n");
    }
    send.status("200 OK")->done(res);
}
