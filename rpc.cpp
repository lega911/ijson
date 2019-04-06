
#include "rpc.h"
#include <string.h>
#include <uuid/uuid.h>
#include "utils.h"
#include "json.h"


void Connect::on_recv(char *buf, int size) {
    if(status != STATUS_NET) {
        print2("!STATUS_NET", buf, size);
        buffer.add(buf, size);
        return;
    }
    Slice data = Slice(buf, size);
    while(true) {
        Slice line = data.pop_line();
        if(!line.valid()) {
            print2("no line", data.ptr(), data.size());
            break;
        }
        line.rstrip();

        if(line.empty()) {
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
            if(data.size()) {
                buffer.add(data);
                throw error::not_implemented("Next request");
            }
            //print2("header_completed", path);
            this->header_completed();
            break;
        }
        if(http_step == HTTP_START) {
            body.clear();
            id.clear();
            content_length = 0;
            if(this->read_method(line) != 0) {
                cout << "Wrong http header\n";  // TODO: close socket
                this->close();
                return;
            }
            http_step = HTTP_HEADER;
        } else if(http_step == HTTP_HEADER) {
            this->read_header(line);
        }

        //if(!line.empty()) print2("line", line.ptr(), line.size());
    }

    if(http_step == HTTP_REQUEST_COMPLETED) {
        http_step = HTTP_START;
    } else if(http_step == HTTP_READ_BODY) {
        throw error::not_implemented("Not implemented: body is not read");
    } else {
        throw Exception(31, "Error reading http header");
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
        content_length = 0;
        for(int i=0;i<data.size();i++) {
            content_length = content_length * 10 + data.ptr()[i] - '0';
        }
    } else if(data.starts_with("Name: ")) {
        data.remove(6);
        name.set(data);
    } else if(data.starts_with("Id: ")) {
        data.remove(4);
        id.set(data);
    }
}


void Connect::header_completed() {
    this->keep_alive = http_version == 11;
    
    if(this->path.equal("/echo")) {
        Buffer b;
        b.add("ok");
        this->send("200 OK", &b);
        return;
    }

    Slice method(this->path);
    Slice name(this->name);
    Slice id(this->id);

    if(method.equal("/rpc/call")) {
        JsonParser json;
        json.parse_object(this->body);

        if(json.method.empty()) throw Exception("Wrong body");
        method = json.method;

        if(method.equal("/rpc/add")) {
            if(name.empty() && json.params.valid()) {
                JsonParser params;
                params.parse_object(json.params);
                name = params.name;
            }
        } else {
            if(id.empty()) {
                id = json.id;
            }
        }
    }

    RpcServer *server = (RpcServer*)this->server;
    if(method.equal("/rpc/add")) {
        if(name.empty()) {
            this->send("400 No name");
            return;
        }
        server->add_worker(name, this);
        return;
    } else if(method.equal("/rpc/result")) {
        int r = server->worker_result(id, this);
        if(r == 0) {
            this->send("200 OK");
        } else {
            // error, no such client
            this->send("400 Wrong id");
        }
        status = STATUS_NET;
        return;
    } else if(method.equal("/rpc/details")) {
        send_details();
        return;
    } else if(method.equal("/rpc/help")) {
        send_help();
        return;
    }

    int r = server->client_request(method, this, id);
    if(r == 0) {
        status = STATUS_WAIT_RESPONSE;
    } else if(r == -1) {
        this->send("404 Not Found");
    } else if(r == -2) {
        this->send("400 No Id");
    } else if(r == -3) {
        this->send("400 Collision Id");
    }
}

void Connect::send(const char *http_status) {
    this->send(http_status, NULL, NULL);
}

void Connect::send(const char *http_status, Buffer *body) {
    this->send(http_status, NULL, body);
}

void Connect::send(const char *http_status, ISlice *id, Buffer *body) {
    send_buffer.resize(256);

    //Buffer r(256);
    send_buffer.add("HTTP/1.1 ");
    send_buffer.add(http_status);
    if(this->keep_alive) {
        send_buffer.add("\r\nConnection: keep-alive");
    }
    if(id) {
        send_buffer.add("\r\nId: ");
        send_buffer.add(id);
    }
    int body_size = 0;
    if(body && body->size()) body_size = body->size();
    send_buffer.add("\r\nContent-Length: ");
    send_buffer.add_number(body_size);
    send_buffer.add("\r\n\r\n");
    
    if(body_size) {
        send_buffer.add(body);
    }

    this->write_mode(true);
}

void Connect::on_send() {
    if(send_buffer.size()) {
        int sent = this->raw_send(send_buffer.ptr(), send_buffer.size());
        if(sent < 0) throw "Not implemented: sent < 0";
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
            std::cout << "dead client\n";
            client = NULL;
            continue;
        }
        sid = client->id.as_string();
        if(wait_response[sid] != NULL) {
            // colision id
            client->send("400 Collision Id");
            client->status = STATUS_NET;
            client = NULL;
            continue;
        }
        break;
    }

    if(client) {
        worker->send("200 OK", &client->id, &client->body);
        wait_response[sid] = client;
        client->link();
        worker->status = STATUS_NET;
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

int RpcServer::client_request(ISlice name, Connect *client, Slice id) {
    MethodLine *ml = this->methods[name.as_string()];
    if(ml == NULL) return -1;
    
    char _uuid[40];
    if(id.empty()) {
        uuid_t uuid;
        uuid_generate_time_safe(uuid);
        //client->id.resize(37, 36);
        uuid_unparse_lower(uuid, _uuid);
        id.set(_uuid, 36);
    }

    Connect *worker = NULL;
    while (ml->workers.size()) {
        worker = ml->workers.front();
        ml->workers.pop_front();
        worker->unlink();
        if(worker->is_closed() || worker->status != STATUS_WAIT_JOB) {
            // TODO: close wrong connection?
            std::cout << "dead worker\n";
            worker = NULL;
            continue;
        }
        break;
    };
    if(worker) {
        string sid = id.as_string();
        if(wait_response[sid] != NULL) {
            ml->workers.push_front(worker);
            worker->link();
            return -3;
        }
        worker->send("200 OK", &id, &client->body);
        wait_response[sid] = client;
        client->link();
        worker->status = STATUS_NET;
    } else {
        ml->clients.push_back(client);
        client->link();
        if(client->id.empty()) {
            client->id.set(id);
        }
    }
    return 0;
};

int RpcServer::worker_result(ISlice id, Connect *worker) {
    string sid = id.as_string();
    Connect *client = wait_response[sid];
    if(client == NULL) {
        return -1;
    }
    wait_response.erase(sid);
    client->unlink();
    client->send("200 OK", &id, &worker->body);
    client->status = STATUS_NET;
    
    if(counter_active) {
        counter ++;
        if(counter > 20000) {
            long now = get_time();
            long rps = (long)((double)counter * 1000000.0 / (double)(now - counter_start));
            counter_start = now;
            counter = 0;
            cout << rps << " rps\n";
        }
    }

    return 0;
};

void Connect::send_details() {
    RpcServer *server = (RpcServer*)this->server;
    Buffer res(256);
    res.add("{");
    for(const auto &it : server->methods) {
        string name = it.first;
        MethodLine *ml = it.second;
        res.add("\"");
        res.add(name.data(), name.size());
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
    send("200 OK", &res);
}

void Connect::send_help() {
    RpcServer *server = (RpcServer*)this->server;
    Buffer res(256);
    for(const auto &it : server->methods) {
        string name = it.first;
        MethodLine *ml = it.second;
        res.add(name.data(), name.size());
        res.add("  x ");
        res.add_number(ml->workers.size());
        res.add("\n");
    }
    send("200 OK", &res);
}
