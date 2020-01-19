
#pragma once

#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include "utils.h"
#include "mapper.h"
#include "workerlist.h"


#define MAX_EVENTS 256
#define BUF_SIZE 16384

class Loop;
class Connect;


class Message {
public:
    int priority = 0;
    Buffer required_worker;
    Connect *conn = NULL;
    Buffer *buf = NULL;
    Buffer name;

    void attach_client(Connect *n_conn);
    void attach_buffer(Buffer *n_buf);
    ~Message();
};


class Queue {
public:
    std::deque<Connect*> workers;
    std::deque<Message*> clients;
};


class QueueLine {
public:
    Buffer name;
    long last_worker = 0;
    std::mutex mutex;
    Queue *queue;
    Buffer info;

    WorkerList workers;
    QueueLine(int n) {
        queue = new Queue[n];
    }
    ~QueueLine() {
        delete[] queue;
    }
};


class Server {
private:
    int _fd;
    int _listen();
    void _accept();
    bool _valid_ip(u32 ip);
public:
    int active_loop = 0;
    int max_fd = 0;
    Slice host;
    int log = 0;
    int port = 8001;
    int threads = 1;
    bool jsonrpc2 = false;
    int fake_fd = 0;
    std::vector<NetFilter> net_filter;
    Connect *connections[MAX_EVENTS];
    Loop **loops;
    std::mutex global_lock;
    u64 stat_connect = 0;

    std::mutex _free_lock;
    std::vector<char*> _free_list;

    Server() : _mapper(this) {
        memset(connections, 0, MAX_EVENTS * sizeof(Connect*));
    };

    void start();
    Lock autolock(int except=-1);

    Mapper _mapper;
    std::vector<QueueLine*> _queue_list;
    QueueLine *get_queue(ISlice key, bool create=false);
    void delete_queue(ISlice key);

    std::map<std::string, Connect*> wait_response;
    std::mutex wait_lock;

    u64 stat_starttime;
};


class Loop {
private:
    int epollfd;
    int _nloop;
    std::thread _thread;

    void _loop();
    void _loop_safe();
    void _close(int fd);
    void _delete_connections();
    void _migrate_send();
    void _migrate_recv();
public:
    bool accept_request = false;
    Server *server;
    std::vector<Connect*> dead_connections;
    std::mutex del_lock;

    Loop(Server *server, int nloop);
    void start();
    void accept(Connect *conn);
    void set_poll_mode(int fd, int status);
    void wake();
    inline auto get_id() {return _thread.native_handle();}

// rpc
private:
    int _add_worker(Slice name, Connect *worker);
public:
    void on_disconnect(Connect *conn);
    void add_worker(ISlice name, Connect *worker);
    int client_request(ISlice name, Connect *client);
    int worker_result(ISlice id, Connect *worker);
    int worker_result_noid(Connect *worker);
    void migrate(Connect *w, Connect *c);

    std::vector<Connect*> _queue_to_send;
    void _perform_to_send();

    u64 stat_request = 0;
    u64 stat_call = 0;
    u64 stat_result = 0;
    u64 stat_recv = 0;
    u64 stat_send = 0;
    u64 stat_ioevent = 0;
};
