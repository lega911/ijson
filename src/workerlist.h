
#pragma once

#include <mutex>


class Connect;
class WorkerList;

class WorkerItem {
public:
    WorkerList *list = NULL;
    WorkerItem *prev = NULL;
    WorkerItem *next = NULL;
    Connect *conn = NULL;

    void pop();
    WorkerItem(WorkerList *n_list, Connect *n_conn);
    ~WorkerItem();
};


class WorkerList {
public:
    std::mutex lock;
    WorkerItem *head = NULL;
    WorkerItem *push(Connect *conn);
    ~WorkerList();
};
