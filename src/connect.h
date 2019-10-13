
#pragma once

#include "server.h"
#include "utils.h"
#include "json.h"
#include "workerlist.h"


enum class Status {
    net, worker_wait_job, worker_wait_result, migration, client_wait_result, busy, worker_mode_async
};

enum class RequestType {
    none = 0,
    get = 1,
    get_plus = 2,
    worker = 3,
    async = 4,
    pub = 5,
    result = 6
};

#define HTTP_START 0
#define HTTP_HEADER 1
#define HTTP_READ_BODY 2
#define HTTP_REQUEST_COMPLETED 3


class DirectMessage {
private:
    std::mutex lock;
public:
    Buffer data;
    int priority = 0;
    std::atomic<u32> counter;
    DirectMessage() : counter(0) {};
    void link() {
        counter++;
    };
    u32 unlink() {
        return --counter;
    };
};


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

    Connect(Server *server, int fd, u64 _id) : connection_id(_id) {
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
    void _set_type(ISlice &name);
public:
    Buffer name;
    RequestType type = RequestType::none;
    Status status = Status::net;
    Buffer body;
    Buffer id;
    bool fail_on_disconnect;
    bool noid;
    bool no_response = false;
    bool worker_mode = false;
    int priority = 0;
    u64 connection_id;
    u64 required_worker = 0;
    Connect *client = NULL;
    Json json;
    Slice info;
    WorkerItem *worker_item = NULL;
    Buffer worker_sub_name;
    std::deque<DirectMessage*> direct_message;

    int read_method(Slice &line);
    void read_header(Slice &data);
    void send_details();
    void send_help();
    void rpc_add();
    void rpc_result(ISlice &id);
    void pub(ISlice &name);
    void header_completed();
};
