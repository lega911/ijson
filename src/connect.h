
#pragma once

#include "server.h"
#include "utils.h"
#include "json.h"


enum class Status {
    net, worker_wait_job, worker_wait_result, migration, client_wait_result, busy, worker_mode_async
};


#define HTTP_START 0
#define HTTP_HEADER 1
#define HTTP_READ_BODY 2
#define HTTP_REQUEST_COMPLETED 3


class HttpSender {
private:
    Connect *conn = NULL;
public:
    HttpSender() {};
    void set_connect(Connect *n_conn) {this->conn = n_conn;};
    HttpSender *status(const char *status);
    HttpSender *header(const char *key, const ISlice &value);
    void done(ISlice &body);
    void done(int error);
    void done();
};


class Connect {
private:
    int _socket_status = 1;  //  1 - read, 2 - write, -1 - closed
    int _socket_status_active = 1;
    int _link = 0;
public:
    int fd;
    bool keep_alive;
    HttpSender send;
    Buffer send_buffer;
    Loop *loop;
    int nloop = 0;
    int need_loop = 0;
    bool go_loop = false;
    Server *server;
    std::mutex mutex;

    Connect(Server *server, int fd) {
        this->server = server;
        this->fd = fd;
        loop = server->loops[0];
        send.set_connect(this);
    };
    ~Connect() {
        fd = 0;
        send.set_connect(NULL);
    };

    void read_mode(bool active);
    void write_mode(bool active);
    inline void _switch_mode() {
        if(_socket_status == _socket_status_active) return;
        loop->set_poll_mode(fd, _socket_status);
        _socket_status_active = _socket_status;
    }

    void close() {_socket_status = -1;};
    inline bool is_closed() {return _socket_status == -1;};
    inline int get_socket_status() {return _socket_status;};

    int get_link() { return _link; }
    void link() { _link++; };
    void unlink();

    void on_recv(char *buf, int size);
    void on_send();
    int raw_send(const void *buf, uint size);

private:
    int http_step = HTTP_START;
    int content_length;
    int http_version;  // 10, 11
    Buffer buffer;
    Buffer path;
    Slice header_option;
public:
    Buffer name;
    Status status = Status::net;
    Buffer body;
    Buffer id;
    bool fail_on_disconnect;
    bool noid;
    bool no_response = false;
    bool worker_mode = false;
    int priority = 0;
    Connect *client = NULL;
    Json json;
    Slice info;

    int read_method(Slice &line);
    void read_header(Slice &data);
    void send_details();
    void send_help();
    void rpc_add();
    void rpc_worker();
    void header_completed();
};
