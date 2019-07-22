
#ifndef CONNECT_H
#define CONNECT_H

#include "server.h"
#include "utils.h"
#include "json.h"


#define HTTP_START 0
#define HTTP_HEADER 1
#define HTTP_READ_BODY 2
#define HTTP_REQUEST_COMPLETED 3

#define STATUS_NET 21
#define STATUS_WORKER_WAIT_JOB 22
#define STATUS_WORKER_WAIT_RESULT 24
#define STATUS_MIGRATE_REQUEST 30

#define CLIENT_WAIT_RESULT 23
#define CONNECT_BUSY 41


class HttpSender {
private:
    Connect *conn;
    bool _autosend;
public:
    HttpSender() : conn(NULL), _autosend(true) {};
    void set_connect(Connect *n_conn) {this->conn = n_conn;};
    HttpSender *status(const char *status);
    HttpSender *header(const char *key, ISlice &value);
    void done(ISlice &body);
    void done(int error);
    void done();
    HttpSender *autosend(bool active=true) {
        _autosend = active;
        return this;
    };
};


class Connect {
private:
    int _socket_status;  //  1 - read, 2 - write, -1 - closed
    int _link;
public:
    int fd;
    bool keep_alive;
    HttpSender send;
    Buffer send_buffer;
    Loop *loop;
    int nloop;
    int need_loop;
    bool go_loop;
    Server *server;
    std::mutex mutex;

    Connect(Server *server, int fd) {
        _socket_status = 1;
        this->server = server;
        this->fd = fd;
        _link = 0;
        nloop = 0;
        need_loop = 0;
        go_loop = false;
        worker_mode = false;
        loop = server->loops[0];
        send.set_connect(this);

        http_step = HTTP_START;
        status = STATUS_NET;
        client = NULL;
    };
    ~Connect() {
        fd = 0;
        send.set_connect(NULL);
    };

    void write_mode(bool active);
    void read_mode(bool active);

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
    int http_step;
    int content_length;
    int http_version;  // 10, 11
    Buffer buffer;
    Buffer path;
    Slice header_option;
public:
    Buffer name;
    int status;
    Buffer body;
    Buffer id;
    bool fail_on_disconnect;
    bool noid;
    bool worker_mode;
    Connect *client;
    JData jdata;

    int read_method(Slice &line);
    void read_header(Slice &data);
    void send_details();
    void send_help();
    void rpc_add();
    void rpc_worker();

    void header_completed();
    void gen_id();
};

#endif  /* CONNECT_H */
