
#ifndef BUFFER_H
#define BUFFER_H

#include <string.h>
#include <iostream>
#include <stddef.h>
#include "memory.h"
#include "exception.h"


class Slice;
class Buffer;

class ISlice {
protected:
    char *_ptr;
    int _size;
public:
    ISlice() {
        reset();
    }
    ~ISlice() {
        reset();
    }
    void reset() {
        _ptr = NULL;
        _size = 0;
    }

    bool starts_with(const char *s) {
        if(s[0] == 0) return true;
        if(size() == 0) return false;
        if(s[0] != _ptr[0]) return false;

        int len = strlen(s);
        if(len > size()) return false;
        return memcmp(ptr(), s, len) == 0;
    }

    bool equal(const char *s) {
        int len = strlen(s);
        if(len != size()) return false;
        return memcmp(s, _ptr, size()) == 0;
    }

    inline bool empty() {return size() == 0;}
    inline bool valid() {return _ptr != NULL;}
    inline char* ptr() {
        if(_ptr == NULL) throw error::NoData();
        return _ptr;
    }
    inline int size() {return _size;}
    std::string as_string() {
        return as_string(5);
    }
    std::string as_string(int _default) {
        std::string s;
        if(valid()) {
            s.append(ptr(), size());
        } else {
            if(_default == 1) s.append("<empty>");
            else if(_default != 0) throw error::NoData();
        }
        return s;
    }
};


class Slice : public ISlice {
public:
    Slice() {
        _ptr = NULL;
        _size = 0;
    }
    Slice(const char *ptr, int size) {
        _ptr = (char*)ptr;
        _size = size;
    }
    Slice(const char *ptr) {
        _ptr = (char*)ptr;
        _size = strlen(ptr);
    }
    Slice(ISlice &s) {
        if(s.valid()) set(s.ptr(), s.size());
    };
    ~Slice() {
        _ptr = NULL;
        _size = 0;
    }
    void set(ISlice &s) {
        set(s.ptr(), s.size());
    }
    void set(const char *ptr, int size) {
        this->_ptr = (char*)ptr;
        _size = size;
    }
    void set(const char *ptr) {
        this->_ptr = (char*)ptr;
        _size = strlen(ptr);
    }
    void clear() {
        set(NULL, 0);
    }

    Slice get(int len) {
        if(len > _size) throw error::ArgumentError();
        return Slice(ptr(), len);
    }
    Slice pop(int len) {
        Slice r = get(len);
        _ptr += len;
        _size -= len;
        return r;
    }

    void remove(int len) {
        if(len > size()) len = size();
        _ptr += len;
        _size -= len;
    }

    Slice pop_line() {
        for(int i=0;i<size();i++) {
            if(_ptr[i] != '\n') continue;
            return pop(i + 1);
        }
        return Slice(NULL, 0);
    }
    void rstrip() {
        int i=_size-1;
        for(;i>=0;i--) {
            if(_ptr[i] == ' ' || _ptr[i] == '\n' || _ptr[i] == '\r') continue;
            break;
        }
        _size = i + 1;
    }
    int atoi() {
        char *p = ptr();
        if(!_size) throw error::NoData();
        int value = 0, i = 0, n;
        bool negative = false;
        if(p[0] == '-') {
            negative = true;
            i = 1;
            if(_size < 2) throw error::InvalidData();
        }
        for(;i<_size;i++) {
            n = p[i] - '0';
            if(n < 0 || n > 9) throw error::InvalidData();
            value = value * 10 + n;
        }
        if(negative) return -value;
        return value;
    }
};


class Buffer : public ISlice {
protected:
    int _cap;

    int _get_cap(int size) {
        int cap = 16;
        int step = 16;
        while(cap < size) {
            cap += step;
            if(step < 4096) {
                step *= 2;
            }
        }
        return cap;
    }
public:
    Buffer() : ISlice() {
        _cap = 0;
        _size = 0;
    };
    Buffer(int size) : ISlice() {
        _cap = 0;
        _size = 0;
        resize(size);
    };
    Buffer(const char *s) : ISlice() {
        _cap = 0;
        _size = 0;
        add(s);
    };
    ~Buffer() {
        if(_ptr) _free(_ptr);
        _ptr = NULL;
    }

    void resize(int capacity) {
        if(_ptr == NULL) {
            _cap = _get_cap(capacity);
            _ptr = (char*)_malloc(_cap);
            if(_ptr == NULL) throw error::NoMemory();
        } else if(capacity > _cap) {
            _cap = _get_cap(capacity);
            _ptr = (char*)_realloc(_ptr, _cap);
        }
    }
    void resize(int capacity, int size) {
        resize(capacity);
        _size = size;
    }

    void add(const char *buf, int size) {
        if(buf == NULL) throw error::NoData("Buffer.add buf == NULL");
        resize(_size + size);
        memmove(&_ptr[_size], buf, size);
        _size += size;
    }
    void add(std::string &s) {
        add(s.data(), s.size());
    }
    void add(ISlice &s) {
        add(s.ptr(), s.size());
    }
    void add(ISlice *s) {
        add(s->ptr(), s->size());
    }
    void add(const char *i) {
        while(*i != 0) {
            if(_size >= _cap) resize(_size + 1);
            _ptr[_size++] = *i;
            i++;
        }
    }
    void add_number(int n) {
        resize(_size + 12);
        
        char s[12];
        int d;
        int i=11;
        bool negative = false;
        if(n == 0) {
            s[i--] = '0';
        } else if(n < 0) {
            n = -n;
            negative = true;
        }
        while(n) {
            d = n % 10;
            s[i--] = d + '0';
            n = n / 10;
        }
        if(negative) {
            s[i--] = '-';
        }
        add(&s[i + 1], 11 - i);
    }
    void set(const char *buf, int size) {
        clear();
        add(buf, size);
    }
    void set(const char *str) {
        clear();
        add(str);
    }
    void set(ISlice &s) {
        set(s.ptr(), s.size());
    }
    void clear() {
        _size = 0;
    }
    void remove_left(int n) {
        if(n <= 0) return;
        if(_size <= n) {
            _size = 0;
        } else {
            _size -= n;
            memcpy(&_ptr[n], _ptr, _size);
        }
    }
};

#endif /* BUFFER_H */
