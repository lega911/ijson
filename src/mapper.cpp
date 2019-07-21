
#include <string.h>
#include "mapper.h"
#include "server.h"


Mapper::Mapper(Server *server) {
    this->server = server;
    _cap = 4;
    _size = 1;
    buf = (char*)_malloc(_cap * sizeof(Step));
    memset(buf, 0, _cap * sizeof(Step));
    buf_t = NULL;
};

u16 Mapper::_next() {
    _size += 1;
    if(_size <= _cap) return _size;

    int old_size = sizeof(Step) * _cap;
    if(_cap >= 512) _cap += 256;
    else _cap *= 2;

    if(buf_t) buf_t = (char*)_realloc(buf_t, _cap * sizeof(Step));
    else {
        buf_t = (char*)_malloc(_cap * sizeof(Step));
        memcpy(buf_t, buf, old_size);
    };
    memset(&buf_t[old_size], 0, sizeof(Step) * _cap - old_size);
    return _size;
};

void Mapper::add(ISlice name, u16 value) {
    mutex.lock();
    int i = 0;
    int next;
    int nstep = 1;
    Step *step = get_step_t(nstep);
    for(;;i++) {
        if(i >= name.size()) {
            step->end = value;
            break;
        }
        u16 a = name.ptr()[i];

        if(a == '*') {
            step->std = value;
            break;
        }

        if(a < 32 || a > 128) throw "Wrong char";
        a -= 32;

        next = step->k[a];
        if(!next) {
            next = _next();
            step = get_step_t(nstep);
            step->k[a] = next;
        }
        nstep = next;
        step = get_step_t(nstep);
    }

    char *old = NULL;
    if(buf_t) {
        old = buf;
        buf = buf_t;
        buf_t = NULL;
    }
    mutex.unlock();

    if(old) {
        server->_free_lock.lock();
        server->_free_list.push_back(old);
        server->_free_lock.unlock();
    }
};

u16 Mapper::find(ISlice name) {
    u16 std = 0;
    int next;
    Step *step = get_step(1);
    for(int i=0;;i++) {
        if(step->std) std = step->std;
        if(i >= name.size()) {
            if(step->end) return step->end;
            return std;
        }
        u16 a = name.ptr()[i];

        if(a < 32 || a > 128) return std;
        a -= 32;

        next = step->k[a];
        if(!next) return std;
        step = get_step(next);
    }
}
