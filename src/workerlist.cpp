
#include "connect.h"
#include "workerlist.h"


WorkerItem::WorkerItem(WorkerList *n_list, Connect *n_conn) : list(n_list), conn(n_conn) {
    conn->link();
};

WorkerItem::~WorkerItem() {
    conn->unlink();
    conn->worker_item = NULL;
};

void WorkerItem::pop() {
    list->lock.lock();
    if(this == list->head) {
        list->head = this->next;
        if(list->head) list->head->prev = NULL;
    } else {
        if(this->prev) this->prev->next = this->next;
        if(this->next) this->next->prev = this->prev;
    }
    list->lock.unlock();
    delete this;
}

WorkerItem *WorkerList::push(Connect *conn) {
    WorkerItem *n = new WorkerItem(this, conn);
    lock.lock();
    if(head) {
        n->next = head;
        head->prev = n;
        head = n;
    } else head = n;
    lock.unlock();
    return n;
}

WorkerList::~WorkerList() {
    while(head) head->pop();
};
