
#include "rpc.h"
#include <string.h>
#include <uuid/uuid.h>
#include "utils.h"
#include "json.h"


void Connect::on_recv(char *buf, int size) {
    //print2("on_recv");
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
            step = NET_REQUEST_COMPLETED;
            if(content_length) {
                int for_read = content_length;
                if(for_read > data.size()) for_read = data.size();
                Slice body_data = data.pop(for_read);
                body.set(body_data);

                if(body.size() < content_length) {
                    step = NET_READ_BODY;
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
        if(step == NET_START) {
            body.clear();
            content_length = 0;
            if(this->read_method(line) != 0) {
                cout << "Wrong http header\n";  // TODO: close socket
                this->close();
                return;
            }
            step = NET_HEADER;
        } else if(step == NET_HEADER) {
            this->read_header(line);
        }

        //if(!line.empty()) print2("line", line.ptr(), line.size());
    }

    if(step == NET_REQUEST_COMPLETED) {
        step = NET_START;
    } else if(step == NET_READ_BODY) {
        throw "Not implemented: body is not read";
    } else {
        throw "Error reading http header";
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

    if(path.size() == 0) return -1;
    //print2("path", path);
    return 0;
}

void Connect::read_header(Slice &data) {
    // Content-Length: 57
    // Id: x15
    // Name: /domain/add
    if(data.starts_with("Content-Length: ")) {
        data.remove(16);
        content_length = 0;
        for(int i=0;i<data.size();i++) {
            content_length = content_length * 10 + data.ptr()[i] - '0';
        }
        //print2("Content-Length", data.ptr(), data.size());
    } else if(data.starts_with("Name: ")) {
        data.remove(6);
        name.set(data);
        //print2("Name", data.ptr(), data.size());
    } else if(data.starts_with("Id: ")) {
        data.remove(4);
        id.set(data);
        //print2("Id", data.ptr(), data.size());
    }
}


void Connect::header_completed() {
    this->keep_alive = http_version == 11;

    RpcServer *server = (RpcServer*)this->server;
    
    if(path.slice().equal("/echo")) {
        Buffer b;
        b.add("ok");
        this->send("200 OK", &b);
        return;
    }

    Slice method = path.slice();
    Slice name = this->name.slice();
    Slice id = this->id.slice();

    if(method.equal("/rpc/call")) {
        JsonParser json;
        json.parse_object(this->body.slice());

        if(!json.method.valid()) throw Exception("Wrong body");
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

    if(method.equal("/rpc/add")) {
        if(!name.valid() || name.empty()) {
            this->send("400 No name");
            return;
        }
        server->add_worker(name, this);
        return;
    } else if(method.equal("/rpc/result")) {
        int r = server->worker_result(&this->id, this);
        if(r == 0) {
            this->send("200 OK");
        } else {
            // error, no such client
            this->send("400 Wrong id");
        }
        status = STATUS_NET;
        return;
    }
    
    std::string key = method.as_string();
    int r = server->client_request(key, this, id);
    if(r == 0) {
        status = STATUS_WAIT_RESPONSE;
    } else if(r == -1) {
        this->send("404 Not Found");
    } else if(r == -2) {
        this->send("400 No Id");
    }
}

void Connect::send(const char *http_status) {
    this->send(http_status, NULL, NULL);
}

void Connect::send(const char *http_status, Buffer *body) {
    this->send(http_status, NULL, body);
}

void Connect::send(const char *http_status, Buffer *id, Buffer *body) {
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

void RpcServer::add_worker(Slice name, Connect *worker) {
    std::string key = name.as_string();
    MethodLine *ml = this->methods[key];
    if(ml == NULL) {
        ml = new MethodLine();
        this->methods[key] = ml;
    }
    Connect *client = NULL;
    while(ml->clients.size()) {
        client = ml->clients.front();
        ml->clients.pop_front();
        if(client->is_closed() || client->status != STATUS_WAIT_RESPONSE) {
            // TODO: close wrong connection?
            std::cout << "dead client\n";
            client = NULL;
            continue;
        }
        break;
    }
    
    if(client) {
        worker->send("200 OK", &client->id, &client->body);
        wait_response[client->id.as_str()] = client;
        worker->status = STATUS_NET;
    } else {
        ml->workers.push_back(worker);
        worker->status = STATUS_WAIT_JOB;
    }
};

int RpcServer::client_request(std::string &name, Connect *client, Slice id) {
    MethodLine *ml = this->methods[name];
    if(ml == NULL) return -1;
    
    char _uuid[40];
    if(id.empty()) {
        uuid_t uuid;
        uuid_generate_time_safe(uuid);
        //client->id.resize(37, 36);
        uuid_unparse_lower(uuid, _uuid);
        id.set(_uuid, 36);
    }
    string id_str = client->id.as_str();
    
    Connect *worker = NULL;
    while (ml->workers.size()) {
        worker = ml->workers.front();
        ml->workers.pop_front();
        if(worker->is_closed() || worker->status != STATUS_WAIT_JOB) {
            // TODO: close wrong connection?
            std::cout << "dead worker\n";
            worker = NULL;
            continue;
        }
        break;
    };
    if(worker) {
        // TODO: use right id
        worker->send("200 OK", &client->id, &client->body);
        wait_response[id_str] = client;
        worker->status = STATUS_NET;
    } else {
        ml->clients.push_back(client);
    }
    return 0;
};

int RpcServer::worker_result(Buffer *id, Connect *worker) {
    string sid = id->as_str();
    Connect *client = wait_response[sid];
    if(client == NULL) {
        return -1;
    }
    wait_response.erase(sid);
    client->send("200 OK", id, &worker->body);
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
